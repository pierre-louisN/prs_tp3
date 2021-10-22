// Server side implementation of UDP client-server model
// source : https://www.geeksforgeeks.org/udp-server-client-implementation-c/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define PORT	 8080
#define PORT2	 1234
#define MAXLINE 1024


int main() {
	int sockfd;
	char buffer[MAXLINE];
	struct sockaddr_in servaddr, cliaddr;
	
	// Creating socket file descriptor
	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}
	
	memset(&servaddr, 0, sizeof(servaddr));
	memset(&cliaddr, 0, sizeof(cliaddr));
	
	// Info du serveur 
	servaddr.sin_family = AF_INET; // IPv4
	servaddr.sin_addr.s_addr = INADDR_ANY; // le serveur prend cette adresse pour être joignable sur n'importe quel interface réseau
	servaddr.sin_port = htons(PORT);
	
	// Bind de la socket sur l'adresse du serveur 
	if ( bind(sockfd, (const struct sockaddr *)&servaddr,
			sizeof(servaddr)) < 0 )
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	
	int len, n;

	len = sizeof(cliaddr);//taille de la structure qui mène vers le client


    n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len);
	buffer[n] = '\0'; 
	printf("Client : %s\n", buffer);
    // char message[64] = "SYN_ACK";
    // char *port = "1234";
    // strcat(message,port);
    
	// sendto(sockfd, (const char *)message, strlen(message), MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);
	// printf("SYN_ACK sent.\n");

    if((strcmp(buffer,"SYN")) == 0){ //si le client s'est bien connecté alors on crée la socket de données
        int data_socket; //on va créer la socket de donnée
        struct sockaddr_in data_addr;
	
        if ( (data_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
            perror("socket creation failed");
            exit(EXIT_FAILURE);
        }
        memset(&data_addr, 0, sizeof(data_addr));
        
        data_addr.sin_family = AF_INET; // IPv4
        data_addr.sin_addr.s_addr = INADDR_ANY; 
        data_addr.sin_port = htons(PORT2);
        
        if (bind(data_socket, (const struct sockaddr *)&data_addr,
                sizeof(data_addr)) < 0 )
        {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }else{
            char message[64] = "SYN-ACK_";
            char port[5];
            // itoa(PORT2, port, 10);
            sprintf(port, "%d", PORT2);
            strcat(message,port);
            sendto(sockfd, (const char *)message, strlen(message), MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);
        }
        n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len);
        buffer[n] = '\0'; 
	    printf("Client : %s\n", buffer);
        if(strcmp(buffer,"ACK") == 0){
            // entrée dans la connexion 
            n = recvfrom(data_socket, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &data_addr, &len);
            buffer[n] = '\0'; 
            printf("Client : %s\n", buffer);
        }

        // n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len);
	    // buffer[n] = '\0'; 
        // printf("Client : %s\n", buffer);
    }

    // while(1){
        

    // }
	
	
	return 0;
}
