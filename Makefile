all: server
server: server.c
	gcc server.c -o server -lm
server2: server_2.c
	gcc server_2.c -o server2 -lm
client: client_udp.c
	gcc client_udp.c -o client_udp
clean: 
	\rm -rf *.o
