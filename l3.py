# encoding=utf8

"""
This step involves writing the controller protocol for the layer 3 routers. The
router works based on the following algorithm:-
  1. Initialize the routing table from the config.json based on the dpid values
  and listen to the core.openflow (i.e. wait for the packet to arrive)

  2. If the received packet is of LLDP type, simply drop it.

  3. If an ARP packet is received, store the source_mac and source_ip in the
  mac_to_ip table. Try sending the waiting packets (the packets for which
  we did not know the mac and enqueued to the lost_buffer queue), based
  on the mac we inserted in the table now. If the packet is an ARP request,
  and the request is for one of the interfaces of the router, then we
  construct the arp packet and send it to back to the requestor.

  4. If any other packet is received, we store the source_mac and store_ip in
  mac_to_ip table and try sending the waiting packets based on the
  1information we learnt now. Now, if the destination IP is in the mac_to_ip
  table, we simply construct a flow rule, which instructs the router to send
  the packet out of the corresponding port (based on the ROUTING Table
  i.e. we are sending to the Next Hop). If the destination IP is not in the
  table, means we have no information about the destination MAC. So, we
  have to send an (rather flood) ARP request in order to get the next hop
  MAC. We enqueue the packet to the lost buffers list while waiting for the
  ARP reply. Once, it is received, we send the packet to the corresponding
  port

"""

from pox.core import core
import pox
log = core.getLogger()

from pox.lib.packet.ethernet import ethernet, ETHER_BROADCAST
from pox.lib.packet.ipv4 import ipv4
from pox.lib.packet.arp import arp
from pox.lib.addresses import IPAddr, EthAddr
from pox.lib.util import str_to_bool, dpid_to_str
from pox.lib.recoco import Timer

import pox.openflow.libopenflow_01 as of

from pox.lib.revent import *

import time
import json

HARD_TOUT=50
IDLE_TOUT=50

# Timeout for flows
FLOW_IDLE_TIMEOUT = 10

# Timeout for ARP entries
ARP_TIMEOUT = 60 * 2

# Maximum number of packet to buffer on a switch for an unknown IP
MAX_BUFFERED_PER_IP = 5

# Maximum time to hang on to a buffer for an unknown IP in seconds
MAX_BUFFER_TIME = 5

routing_table = {}

def dpid_to_mac (dpid):
  return EthAddr("%012x" % (dpid & 0xffFFffFFffFF,))

class Router(EventMixin):
  def __init__(self):
    self.listenTo(core.openflow)

  def _handle_ConnectionUp (self, event):
    log.debug("Connection %s" % (event.connection,))
    l3_switch(event.connection)

  def _handle_ConnectionDown(self, event):
    print "Connection Down"
    routing_table = {}

class l3_switch(EventMixin):
  def __init__(self,connection):
    # Initializing Controller
    dpid = connection.dpid
    print dpid
    # print connection.ports.values()
    self.connection=connection

    # we can't deliver these buffres because we don't know their destination
    self.lost_buffers = {}

    # mapping mac to ip
    self.mac_to_ip={}

    # Loading Routing table from config file
    config = json.loads(open('config.json').read())
    for datapath_id in config:
      for d_id in datapath_id.keys():
        d_id = d_id.encode('ascii', 'ignore')
        if int(d_id) == dpid:
          routing_table[dpid] = {}  
          for row in datapath_id[d_id]:
            data = datapath_id[d_id][row]
            dest = data["destination"].encode('ascii', 'ignore')
            routing_table[dpid][dest] = []
            routing_table[dpid][dest].append(data["next_hop"].encode('ascii', 'ignore'))
            routing_table[dpid][dest].append(data["interface"].encode('ascii', 'ignore'))
    print routing_table
    print "\n"
    self.listenTo(connection)
  
  def _send_lost_buffers (self, dpid, ipaddr, macaddr, port):
    if (dpid,ipaddr) in self.lost_buffers:
      bucket = self.lost_buffers[(dpid,ipaddr)]
      del self.lost_buffers[(dpid,ipaddr)]
      log.debug("Sending %i buffered packets to %s from %s"
                % (len(bucket),ipaddr,dpid_to_str(dpid)))
      for buffer_id,in_port in bucket:
        po = of.ofp_packet_out(buffer_id=buffer_id,in_port=in_port)
        po.actions.append(of.ofp_action_dl_addr.set_dst(macaddr))
        po.actions.append(of.ofp_action_output(port = port))
        core.openflow.sendToDPID(dpid, po)  

  def _handle_PacketIn (self, event):
    dpid = event.connection.dpid
    inport = event.port
    packet = event.parsed

    # Ignore LLDP packets
    if packet.type == ethernet.LLDP_TYPE:
      return

    if isinstance(packet.next, arp):
      # ARP packet
      a = packet.next
      inport = event.port
      log.debug("%i %i ARP %s %s => %s", dpid, inport,
       {arp.REQUEST:"request",arp.REPLY:"reply"}.get(a.opcode,
       'op:%i' % (a.opcode,)), str(a.protosrc), str(a.protodst))

      print packet.next
      if a.prototype == arp.PROTO_TYPE_IP:
        if a.hwtype == arp.HW_TYPE_ETHERNET:
          if a.protosrc != 0:
            # Learn or update port/MAC info
            if a.protosrc in self.mac_to_ip:
              log.debug("IP present in mac to ip table")
            else:
              # print a.hwsrc
              self.mac_to_ip[a.protosrc] = a.hwsrc
            # print self.mac_to_ip

            # Send any waiting packets...
            self._send_lost_buffers(dpid, a.protosrc, packet.src, inport)       

            if a.opcode == arp.REQUEST:
              # Maybe we can answer
              if a.protodst in self.mac_to_ip or a.hwdst == EthAddr("00:00:00:00:00:00"):
                # We have an answer...
                # print "..........."
                # Creating new ARP packet
                r = arp()
                r.hwtype = a.hwtype
                r.prototype = a.prototype
                r.hwlen = a.hwlen
                r.protolen = a.protolen
                r.opcode = arp.REPLY
                r.hwdst = a.hwsrc
                r.protodst = a.protosrc
                r.protosrc = a.protodst
                for entry in event.connection.ports.values():
                  port = entry.port_no
                  if port == inport:  
                    mac = entry.hw_addr
                
                r.hwsrc = mac
                e = ethernet(type=packet.type, src=dpid_to_mac(dpid),
                             dst=a.hwsrc)
                e.set_payload(r)
                log.debug("%i %i answering ARP for %s" % (dpid, inport,str(r.protosrc)))
                # Creating new message
                msg = of.ofp_packet_out()
                msg.data = e.pack()
                msg.actions.append(of.ofp_action_output(port = of.OFPP_IN_PORT))
                msg.in_port = inport
                event.connection.send(msg)
                return
    else:   
      # print "Ipv4"         
      log.debug("%i %i IP %s => %s", dpid,inport,
                packet.next.srcip,packet.next.dstip)
      inport = event.port
      # Send any waiting packets...
      self._send_lost_buffers(dpid, packet.next.srcip, packet.src, inport)          
      
      # Learn or update port/MAC info
      print packet.next.srcip
      print packet.src
      if packet.next.srcip in self.mac_to_ip:
        log.debug("IP present in mac to ip table")
      else:
        self.mac_to_ip[packet.next.srcip] = packet.src

      # Try to forward
      dstaddr = packet.next.dstip
      if dstaddr in self.mac_to_ip:
        # We have info about what port to send it out on...
        # print "Checking dstaddr..."
        prt = int(routing_table[dpid][str(dstaddr)][1])
        mac = self.mac_to_ip[dstaddr]
        print inport, prt, mac

        # Creating new Message
        actions = []
        actions.append(of.ofp_action_dl_addr.set_dst(mac))
        actions.append(of.ofp_action_output(port = prt))
        match = of.ofp_match.from_packet(packet, inport)
        match.dl_src = None # Wildcard source MAC
        match.dl_dst = mac;

        msg = of.ofp_flow_mod(command=of.OFPFC_ADD,
                              idle_timeout=FLOW_IDLE_TIMEOUT,
                              hard_timeout=of.OFP_FLOW_PERMANENT,
                              buffer_id=event.ofp.buffer_id,
                              actions=actions,
                              match=match)

        # print "Message: "
        event.connection.send(msg)
      else:
        # We don't know this destination.
        # First, we track this buffer so that we can try to resend it later
        # if we learn the destination, second we ARP for the destination,
        # which should ultimately result in it responding and us learning
        # where it is

        # Add to tracked buffers
        if (dpid,dstaddr) not in self.lost_buffers:
          self.lost_buffers[(dpid,dstaddr)] = []
        bucket = self.lost_buffers[(dpid,dstaddr)]
        entry = (event.ofp.buffer_id,inport)
        bucket.append(entry)
        # print "Flood..."

        r = arp()
        r.hwtype = r.HW_TYPE_ETHERNET
        r.prototype = r.PROTO_TYPE_IP
        r.hwlen = 6
        r.protolen = r.protolen
        r.opcode = r.REQUEST
        r.hwdst = ETHER_BROADCAST
        r.protodst = dstaddr
        r.hwsrc = packet.src
        r.protosrc = packet.next.srcip
        e = ethernet(type=ethernet.ARP_TYPE, src=packet.src,
                     dst=ETHER_BROADCAST)
        e.set_payload(r)
        log.debug("%i %i ARPing for %s on behalf of %s" % (dpid, inport,
         str(r.protodst), str(r.protosrc)))
        msg = of.ofp_packet_out()
        msg.data = e.pack()
        msg.actions.append(of.ofp_action_output(port = of.OFPP_FLOOD))
        msg.in_port = inport
        event.connection.send(msg)

def launch ():
  core.registerNew(Router)
