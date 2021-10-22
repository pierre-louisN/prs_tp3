// Client side implementation of UDP client-server model
// source : https://www.geeksforgeeks.org/udp-server-client-implementation-c/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define PORT	 8080 // numéro de port
#define MAXLINE 1024 // taille max des message qu'on peut recevoir 


int main() {
	int sockfd; // socket du client
	char buffer[MAXLINE]; // tableau pour recevoir des messages
	struct sockaddr_in servaddr; // structure qui va contenir notre socket 
	// Création de la socket (descripteur de fichier )
	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}

	memset(&servaddr, 0, sizeof(servaddr)); // créer l'espace mémoire pour stocker l'adresse
	
	// Information du serveur 
	servaddr.sin_family = AF_INET; //IPv4
	servaddr.sin_port = htons(PORT); // port de la socket par laquelle on va passer
	servaddr.sin_addr.s_addr = INADDR_ANY; // le serveur prend n'importe quelle adresse
	
	int n, len;
    // début du three way hanshake
    char *message = "SYN"; // message de connexion
    sendto(sockfd, (const char *)message, strlen(message), MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));
	printf("SYN envoyé\n");

	// WAITALL signifie que la socket va être bloqué tant qu'elle ne recoit pas la totalité des données
    // recvfrom retourne la taille du message en octets, 0 si on attend trop longtemps et qu'il n'y a pas de message
	// servaddr est l'addr où on envoie les données
    //recvfrom est une fonction bloquante, donc ce n'est pas nécessaire de faire un autre boucle par dessus 
    n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &servaddr, &len);
	buffer[n] = '\0'; // signifie la fin d'un string (ici le message)
	printf("Server : %s\n", buffer);

    char delim[] = "_";
    char *ptr = strtok(buffer, delim);
    printf("'%s'\n", ptr);
    if(strcmp(ptr,"SYN-ACK")==0){
        ptr = strtok(NULL, delim); // le serveur nous donne le numéro de port de sa nouvelle socket
        int socket_serv_data = atoi(ptr);
        printf("data socket port : %d\n", socket_serv_data);
        message = "ACK"; // message de test
        sendto(sockfd, (const char *)message, strlen(message), MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));
        printf("ACK envoyé\n");

        // fin du three way handshake, on envoi des données sur le port de la socket data
        char *filename = "test.txt"; // nom du fichier qu'on souhaite avoir
        //servaddr.sin_port = htons(ptr);
        printf("%d %s %d\n",socket_serv_data,inet_ntoa(servaddr.sin_addr),ntohs(servaddr.sin_port));
        servaddr.sin_port = htons(socket_serv_data);
        int n2 = sendto(sockfd, (const char *)filename, strlen(filename), MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));
        if(n2<0){
            perror("error sendto");
        }
        printf("filename envoyé %d\n",n2);


    }
    
    



	close(sockfd);
	return 0;
}
