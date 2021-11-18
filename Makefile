all: server
server: server.c
	gcc server.c -o server -lm
client: client_udp.c
	gcc client_udp.c -o client_udp
clean: 
	\rm -rf *.o
