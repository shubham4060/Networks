# encoding=utf8

from pox.core import core                     # Main POX object
import pox.openflow.libopenflow_01 as of      # OpenFlow 1.0 library
from pox.lib.addresses import EthAddr, IPAddr # Address types
import pox.lib.recoco as recoco
from pox.lib.revent import *
from pox.lib.util import dpidToStr
from pox.lib.packet import arp, ethernet

"""
This step involves writing the controller protocol for the layer 2 switches. The
switch works based on the following algorithm:-
1. When a packet is received, the source mac and the in-port is stored in the
mac_to_interface table.
2. Then, if destination mac is already present in the mac_to_interface table,
we create a flow rule corresponding to source_mac->destination_mac as
well as destination_mac->source_mac with some timeout values.
3. Else, if we don’t know the destination mac, we simply FLOOD the packet
across the network. No flow rules are created here.
"""


# Create a logger for this component
log = core.getLogger()
HARD_TOUT=50
IDLE_TOUT=50

class Switch(EventMixin):
  def __init__(self):
    self.listenTo(core.openflow)

  def _handle_ConnectionUp (self, event):
    log.debug("Connection %s" % (event.connection,))
    l2_switch(event.connection)

class l2_switch(EventMixin):

  def __init__(self,connection):
    # Switch we'll be adding L2 learning switch capabilities to
    self.connection=connection

    # mapping map addresses to ports
    self.mac_to_interface={}
    
    self.listenTo(connection)

  def send_packet(self,id,data,dst,src):
    # Here we will make output packet
    packet=of.ofp_packet_out()
    packet.in_port=src

    if id!=-1 and id is not None:
      packet.buffer_id=id
    else:
      packet.data=data

    action=of.ofp_action_output(port=dst)
    packet.actions.append(action)
    self.connection.send(packet)

  def _handle_PacketIn (self, event):
    packet=event.parse()
    packet_in=event.ofp

    self.mac_to_interface[packet.src]=event.port

    # Findind destination port using mac
    destination=self.mac_to_interface.get(packet.dst)

    if destination is None:
      # Destination(Port) is not present in table
      self.send_packet(packet_in.buffer_id, packet_in.data, of.OFPP_FLOOD, packet_in.in_port)
      log.debug("Destination port not matched... Flood %s,%i------>%s,%i" %(packet.src, event.ofp.in_port, packet.dst, of.OFPP_ALL))
    else:
      self.send_packet(packet_in.buffer_id, packet_in.data, destination, packet_in.in_port)

      print destination, packet.dst
      # we create a flow rule corresponding to source_mac->destination_mac as
      # well as destination_mac->source_mac with some timeout values
      flow1=of.ofp_flow_mod()
      flow1.idle_timeout=IDLE_TOUT
      flow1.hard_timeout=HARD_TOUT
      flow1.match.dl_dst=packet.src
      flow1.match.dl_src=packet.dst
      flow1.actions.append(of.ofp_action_output(port=event.port))
      self.connection.send(flow1)


      flow2=of.ofp_flow_mod()
      flow2.idle_timeout=IDLE_TOUT
      flow2.hard_timeout=HARD_TOUT
      flow2.match.dl_dst=packet.dst
      flow2.match.dl_src=packet.src
      flow2.actions.append(of.ofp_action_output(port=destination))
      self.connection.send(flow2)

      log.debug("Installing %s.%i -> %s.%i AND %s.%i -> %s.%i" % (packet.dst, destination, packet.src, event.ofp.in_port, packet.src, event.ofp.in_port, packet.dst, destination))

def launch ():
  core.registerNew(Switch)
