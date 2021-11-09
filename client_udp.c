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
#include <time.h>

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
        //printf("data socket port : %d\n", atoi(ptr));
        message = "ACK"; // message de test
        if(sendto(sockfd, (const char *)message, strlen(message), MSG_DONTWAIT, (const struct sockaddr *) &servaddr, sizeof(servaddr)) > 0){
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
	bzero(buffer,sizeof(buffer));
	len = sizeof(servaddr);

	// fin du three way handshake, on envoi des données sur le port de la socket data
	char *filename = "bigfile.txt"; // nom du fichier qu'on souhaite avoir
	//servaddr.sin_port = htons(ptr);
	//printf("%d %s %d\n",socket_serv_data,inet_ntoa(servaddr.sin_addr),ntohs(servaddr.sin_port));
	servaddr.sin_port = htons(port); // on peut utiliser la même socket mais il faut changer le port 
	int n2 = sendto(sockfd, (const char *)filename, strlen(filename), MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));
	printf("filename envoyé \n");
	//printf("contenu du fichier %s :\n", filename);

	FILE *fp;

	int ack = 1;
	char buffer_file[1024];
	bzero(buffer_file,sizeof(buffer_file));
	fp = fopen("output_file.txt", "w");
	n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &servaddr,&len);
	//buffer[n] = '\0'; // signifie la fin d'un string (ici le message)
	//printf("Server : %s\n", buffer);
	int nbre_octets = n; // on va compter le nombre d'octet qu'on reçoit pour calculer le débit
	char ack_char[7];
	clock_t before = clock(); // on va utiliser un timer pour calculer le débit en divisant le temps par le nombre de paquets
	// clock returns the number of clock ticks elapsed since the start of the program.
	while(strcmp(buffer,"EOF")!=0){ // serveur nous envoi EOF pour nous signifier qu'il a fini, pas bon pour les perfs peut être car strcmp coûte du temps peut être
		//recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &servaddr, &len);
		//n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &servaddr, &len);
		
		buffer[n] = '\0'; // signifie la fin d'un string (ici le message)
		
		//printf("Server : %s\n", buffer);
		char delim[] = "_";
		char *seq_ptr = strtok(buffer, delim);
		//printf("Serveur : '%s'\n", buffer);
		int num_seq = atoi(seq_ptr);
		printf("numéro de séquence reçu : %d\n", num_seq);
		if(ack == 1 || num_seq==ack+1){
			ack = num_seq;
			//printf("ack : %d\n",ack);
			sprintf(ack_char,"%6d",ack);
			sendto(sockfd, (const char *)ack_char, strlen(ack_char), MSG_DONTWAIT, (const struct sockaddr *) &servaddr, sizeof(servaddr));
			
		}
		seq_ptr = strtok(NULL, delim);
		fwrite(seq_ptr,1,n-(sizeof(ack_char)),fp); // on enlève 7 octet car les 7 premiers octets ne sont pas utiles
		//fputs(buffer, fp);
		//printf("buffer contient : %s\n",buffer_file);
		//strcat(buffer_file,seq_ptr);
		//puts(seq_ptr);
		bzero(buffer,sizeof(buffer)); // vide le buffer
		n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &servaddr, &len);
		nbre_octets +=n;
		// char message[64];
		// sprintf(message, "%d", message_number);
		// int n2 = sendto(sockfd, (const char *)message, strlen(message), MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));
	}
	clock_t difference = clock() - before; // le nombre de tick que prend l'échange du fichier
	//CLOCKS_PER_SEC : nombre de tick fait par le CPU en 1 sec (pour avoir le nombre de secondes)
	long double time = ((long double)difference*1000/(long double)CLOCKS_PER_SEC);
	printf("Durée échange fichier : %Lf millisecondes\n",time);
	printf("Débit : %Lf octets/s\n",(long double)nbre_octets/(time/1000));
	//fputs(buffer_file, fp);
	//fwrite(buffer_file,1,1024,fp);
	printf("Fichier reçu et crée\n");
	fclose(fp);
}

// void exchange_data(int port, int sockfd, struct sockaddr_in servaddr){
// 	int n, len;
// 	char buffer_data[MAXLINE];
// 	//bzero(buffer_,sizeof(buffer)); 
// 	len = sizeof(servaddr);
// 	servaddr.sin_port = htons(port); // on peut utiliser la même socket mais il faut changer le port 
// 	int message_number = 1;
// 	while(1){
// 		sleep(2);
// 		char message[64];
// 		sprintf(message, "%d", message_number);
// 		int n2 = sendto(sockfd, (const char *)message, strlen(message), MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));
// 		recvfrom(sockfd, (char *)buffer_data, MAXLINE, MSG_WAITALL, (struct sockaddr *) &servaddr, &len);	
// 		char delim[] = "_";
// 		char *ptr = strtok(buffer_data, delim);
// 		printf("Serveur : '%s'\n", buffer_data);
// 		if(strcmp(ptr,"	ACK")==0){
// 			ptr = strtok(NULL, delim);
// 			int num_ack = atoi(ptr);
// 			printf("numéro de l'ACK reçu : %d\n", num_ack);
// 			if(num_ack==message_number){
// 				message_number++;
// 			}
// 		}
// 	}
// }

int main() {
	int sockfd; // socket du client
	struct sockaddr_in servaddr; // structure qui va contenir notre socket 
    createsocket(&sockfd,&servaddr,PORT);

    int res_twh = thw_client(sockfd,servaddr);
    if(res_twh != 0){
        exchange_file(res_twh, sockfd, servaddr);
		//exchange_data(res_twh,sockfd,servaddr);
    }
	close(sockfd);
	return 0;
}

// à faire : changer le nom des variables, définir send et recv pour les messages d'erreurs;