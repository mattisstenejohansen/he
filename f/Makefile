CCFLAGS= -g

all: mip_daemon ping_client ping_server routing_daemon

mip_daemon:
	gcc mip_daemon.c mip_protocol.c mip_arp.c ping.c routing_table.c routing.c message_queue.c interface.c sockets.c linked_list.c $(CCFLAGS) -o mip_daemon

ping_client:
	gcc ping_client.c ping.c sockets.c $(CCFLAGS) -o ping_client

ping_server:
	gcc ping_server.c ping.c sockets.c $(CCFLAGS) -o ping_server

routing_daemon:
	gcc routing_daemon.c routing_table.c routing.c sockets.c linked_list.c $(CCFLAGS) -o routing_daemon -lpthread

clean:
	rm -f mip_daemon
	rm -f ping_client
	rm -f ping_server
	rm -f routing_daemon
