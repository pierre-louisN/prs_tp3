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

void send_udp();
void recv_udp(); 

// il faut utiliser des pointeurs pour modifier les éléments originaux
void createsocket(int *sockfd, struct sockaddr_in *servaddr, int port){
	// Création de la socket (descripteur de fichier )
	if ( (*sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}
	memset(servaddr, 0, sizeof(servaddr)); // créer l'espace mémoire pour stocker l'adresse
	// Information du serveur 
	servaddr->sin_family = AF_INET; //IPv4
	servaddr->sin_port = htons(port); // port de la socket par laquelle on va passer
	servaddr->sin_addr.s_addr = INADDR_ANY; // le serveur prend n'importe quelle adresse
}

int thw_client(int sockfd, struct sockaddr_in servaddr){
    char buffer[MAXLINE]; // tableau pour recevoir des messages
    int n, len;

	len = sizeof(servaddr);
    // début du three way hanshake
    char *message = "SYN"; // message de connexion
    if(sendto(sockfd, (const char *)message, strlen(message), MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr)) > 0){
        printf("SYN envoyé\n");
    }else{
        return 0;
    }
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
        //int socket_serv_data = atoi(ptr);
        //printf("data socket port : %d\n", socket_serv_data);
        message = "ACK"; // message de test
        if(sendto(sockfd, (const char *)message, strlen(message), MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr)) > 0){
            printf("ACK envoyé\n");
        }else{
            return 0;
        } 
    }else{
        return 0;
    }
    return atoi(ptr);
    
}

void exchange_file(int port, int sockfd, struct sockaddr_in servaddr){
    int n, len;
	char buffer[MAXLINE];

	// fin du three way handshake, on envoi des données sur le port de la socket data
	char *filename = "bigfile.txt"; // nom du fichier qu'on souhaite avoir
	//servaddr.sin_port = htons(ptr);
	//printf("%d %s %d\n",socket_serv_data,inet_ntoa(servaddr.sin_addr),ntohs(servaddr.sin_port));
	servaddr.sin_port = htons(port); // on peut utiliser la même socket mais il faut changer le port 
	int n2 = sendto(sockfd, (const char *)filename, strlen(filename), MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));
	printf("filename envoyé %d\n",n2);
	//printf("contenu du fichier %s :\n", filename);

	FILE *fp;

	fp = fopen("output_file.txt", "w+");
	recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &servaddr,&len);
	while(strcmp(buffer,"EOF")==0){ // serveur nous envoi EOF pour nous signifier qu'il a fini, pas bon pour les perfs peut être car strcmp coûte du temps peut être
		fputs(buffer, fp);
		bzero(buffer,sizeof(buffer)); // vide le buffer
		recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &servaddr, &len);
	}
	printf("Fichier reçu et crée\n");
	fclose(fp);
}

int main() {
	int sockfd; // socket du client
	struct sockaddr_in servaddr; // structure qui va contenir notre socket 
    createsocket(&sockfd,&servaddr,PORT);

    int res_twh = thw_client(sockfd,servaddr);
    if(res_twh != 0){
        exchange_file(res_twh, sockfd, servaddr);
    }
	close(sockfd);
	return 0;
}

// à faire : changer le nom des variables, définir send et recv pour les messages d'erreurs;