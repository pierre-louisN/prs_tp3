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

void createsocket(int *sockfd, struct sockaddr_in *servaddr, int port){
	// Creating socket file descriptor
	if ( (*sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}
	
	memset(servaddr, 0, sizeof(servaddr));
	
	// Info du serveur 
	servaddr->sin_family = AF_INET; // IPv4
	servaddr->sin_addr.s_addr = INADDR_ANY; // le serveur prend cette adresse pour être joignable sur n'importe quel interface réseau
	servaddr->sin_port = htons(port);
	
	// Bind de la socket sur l'adresse du serveur 
	if ( bind(*sockfd, (const struct sockaddr *)servaddr, sizeof(*servaddr)) < 0 )
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
}

int exchange_file(int len, int data_socket, struct sockaddr_in data_addr){
    char buffer[MAXLINE];
    // entrée dans la connexion 
    int n = recvfrom(data_socket, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &data_addr, &len);
    buffer[n] = '\0'; 
    printf("Client : %s\n", buffer);
    FILE *fp;
    char str[64];

    /* opening file for reading */
    fp = fopen(buffer , "r"); // ouverture du fichier
    if(fp == NULL) {
        perror("Error opening file");
        return(-1);
    }
    while( fgets(str, 64, fp)!=NULL ) { // fgets : str => la chaîne qui contient la chaine de caractère lu, 60 : nmbre max de caractère à lire, fr : stream d'où sont lus les chaînes
        /* writing content to stdout */
        //puts(str);
        // on va écrire le texte vers le client 
        sendto(data_socket, (const char *)str, strlen(str), MSG_CONFIRM, (const struct sockaddr *) &data_addr, len);
        //bzero(str,sizeof(str));
    }
    strcpy(str,"EOF");
    sendto(data_socket, (const char *)str, strlen(str), MSG_CONFIRM, (const struct sockaddr *) &data_addr, len);
    fclose(fp);
    return 1;
}

int twh_serv(int sockfd, struct sockaddr_in cliaddr){
    int len, n;
    char buffer[MAXLINE];
	len = sizeof(cliaddr);//taille de la structure qui mène vers le client
    n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len);
	buffer[n] = '\0'; 
	printf("Client : %s\n", buffer);
    if((strcmp(buffer,"SYN")) == 0){ //si le client s'est bien connecté alors on crée la socket de données
        int data_socket; //on va créer la socket de donnée
        struct sockaddr_in data_addr;
    
        createsocket(&data_socket,&data_addr,PORT2);
        
        char message[64] = "SYN-ACK_";
        char port[5];
        // itoa(PORT2, port, 10);
        sprintf(port, "%d", PORT2);
        strcat(message,port);
        sendto(sockfd, (const char *)message, strlen(message), MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);
        
        n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len);
        buffer[n] = '\0'; 
	    printf("Client : %s\n", buffer);
        if(strcmp(buffer,"ACK") == 0){
            if(exchange_file(len,data_socket,data_addr) > 0){
                return 1;
            }
        }
    }
    return 0;

}




int main() {
	int sockfd;
	char buffer[MAXLINE];
	struct sockaddr_in servaddr, cliaddr;
	memset(&cliaddr, 0, sizeof(cliaddr));
    createsocket(&sockfd,&servaddr,PORT);

	int res_twh = twh_serv(sockfd,cliaddr);
    if(res_twh != 0){
        printf("exchange successful \n");
    }
    // while(1){
        

    // }
	
	
	return 0;
}
