 #!/usr/bin/python

from mininet.topo import Topo
from mininet.net import Mininet
from mininet.util import dumpNodeConnections
from mininet.log import setLogLevel
from mininet.node import CPULimitedHost
from mininet.link import TCLink



class MyTopo( Topo ):
	print "Simple topology example."

	def __init__( self ):
		"Create custom topo."
    
		# Initialize topology
		Topo.__init__( self )
		n=0
		host1= self.addHost('h1')
		host2= self.addHost('h2')
		self.addLink(host1,host2, bw=1024,delay='1ms',loss=1)

topos = { 'mytopo': ( lambda: MyTopo() ) }
