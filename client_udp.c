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
#include <sys/time.h>


#define PORT	 8080 // numéro de port
#define MAXLINE 1024 // taille max des message qu'on peut recevoir 

void send_udp();
void recv_udp(); 

void init_struct(struct sockaddr_in *servaddr, int port){
	memset(servaddr, 0, sizeof(servaddr)); // créer l'espace mémoire pour stocker l'adresse
	// Information du serveur : le client doit connaître les infos du serveur pour s'y connecter (son adresse et son port) 
	servaddr->sin_family = AF_INET; //IPv4
	servaddr->sin_port = htons(port); // port de la socket par laquelle on va passer
	//servaddr->sin_addr.s_addr = INADDR_ANY; // le serveur prend n'importe quelle adresse 
	inet_aton("127.0.0.1", &servaddr->sin_addr);
	// mais ici on doit donner une vraie adresse au serveur car 0.0.0.0 est une adresse non routable
}

// il faut utiliser des pointeurs pour modifier les éléments originaux
void createsocket(int *sockfd, struct sockaddr_in *servaddr, int port){
	// Création de la socket (descripteur de fichier )
	if ( (*sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}
	init_struct(servaddr,port);
	 
}

int thw_client(int sockfd, struct sockaddr_in servaddr){
    char buffer[MAXLINE]; // tableau pour recevoir des messages
    int n, len;

	len = sizeof(servaddr);
    // début du three way hanshake
    char *message_syn = "SYN"; // message de connexion
    if(sendto(sockfd, (const char *)message_syn, strlen(message_syn), MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr)) > 0){
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
        char *message_ack = "ACK"; // message de test
		// ici il faut utiliser une autre chaîne de caractère que message car sinon memory leak
        if(sendto(sockfd, (const char *)message_ack, strlen(message_ack), MSG_DONTWAIT, (const struct sockaddr *) &servaddr, sizeof(servaddr)) > 0){
            printf("ACK envoyé\n");
        }else{
            return 0;
        } 
    }else{
        return 0;
    }
    return atoi(ptr);
    
}


char *concat(char const*str1, char const*str2) {
   size_t const l1 = strlen(str1) ;
   size_t const l2 = strlen(str2) ;

    char* result = malloc(l1 + l2 + 1);
    if(!result) return result;
    memcpy(result, str1, l1) ;
    memcpy(result + l1, str2, l2 + 1);
    return result;
}

void generate_ack(char **ack_char, int ack){
	
	*ack_char = "ACK_";
	int i = ack;
	while(i>0){
		*ack_char = concat(*ack_char,"A");
		i--;
	}

}

void exchange_file(int sockfd, struct sockaddr_in servaddr, char *input_file, char *output_file){
    int n, len;
	char buffer[MAXLINE];
	bzero(buffer,sizeof(buffer));
	len = sizeof(servaddr);

	// fin du three way handshake, on envoi des données sur le port de la socket data
	char *filename = input_file;
	
	int n2 = sendto(sockfd, (const char *)filename, strlen(filename), MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));
	printf("filename envoyé \n");

	FILE *fp;

	int ack = 1;
	char buffer_file[1024];
	bzero(buffer_file,sizeof(buffer_file));

	fp = fopen(output_file, "w");
	n = recvfrom(sockfd, buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &servaddr,&len);
	//buffer[n] = '\0'; // signifie la fin d'un string (ici le message)
	//printf("Server : %s\n", buffer);
	int nbre_octets = n-6; // on va compter le nombre d'octet qu'on reçoit pour calculer le débit
	char *ack_char;
	// clock() est une fonction complexe donc on va plutot utiliser gettimeofday()
	struct timeval time_before;
	struct timeval time_after;
	gettimeofday(&time_before,NULL);
	char data[58];
	bzero(data,sizeof(data));
	char char_seq[7];
	char_seq[7] = '\0';
	bzero(char_seq,sizeof(char_seq));
	// le 2e argument est la timezon mais est devenu obsolète
	while(strcmp(buffer,"EOF")!=0){ // serveur nous envoi EOF pour nous signifier qu'il a fini, pas bon pour les perfs peut être car strcmp coûte du temps peut être
		printf("%d octets reçus\n",n); 
		//puts(buffer);
		
		//buffer[n] = '\0'; // signifie la fin d'un string (ici le message)
		
		//printf("Server : %s\n", buffer);
		 // il faut que la taille de ce tableau soit > 6 sinon on a undefined behaviour pour memcpy
		memcpy(char_seq,buffer,6);
		printf("le numéro de séquence est %s\n",char_seq);
		int num_seq = atoi(char_seq);
		printf("numéro de séquence reçu : %d\n",num_seq);
		// if(ack == 1 || num_seq==ack+1){ // si c'est un segment reçu en continu alors on l'ACK
		// 	ack = num_seq;

		// 	generate_ack(&ack_char,ack);
		// 	//printf("ici\n");
		// 	//printf("ack : %s\n",ack_char);
		// 	//sprintf(ack_char,"%6d",ack);
		// 	// on peut utiliser strlen(ack_char) car c'est forcément une chaîne de caractères
		// 	sendto(sockfd, (const char *)ack_char, strlen(ack_char), MSG_DONTWAIT, (const struct sockaddr *) &servaddr, sizeof(servaddr));
		// 	printf("ACK n°%d envoyé\n",ack);
		// }
		// dés qu'on reçoit un message, on l'ACK
		ack = num_seq;
		generate_ack(&ack_char,ack);
		//printf("ici\n");
		//printf("ack : %s\n",ack_char);
		//sprintf(ack_char,"%6d",ack);
		// on peut utiliser strlen(ack_char) car c'est forcément une chaîne de caractères
		sendto(sockfd, (const char *)ack_char, strlen(ack_char), MSG_DONTWAIT, (const struct sockaddr *) &servaddr, sizeof(servaddr));
		printf("ACK n°%d envoyé\n",ack);
		// printf("%c\n",buffer[8]);
		// seq_ptr = strtok(NULL, delim);
		memcpy(data,buffer+6,n);
		printf("data : %s\n",data);
		if(data!=NULL){
			// pour écrire la chaîne au bon endroit, on va se déplacer au bon endroit avec fseek()
			
			fseek(fp,(num_seq-1)*58,SEEK_SET);			
			fwrite(data,1,n-6,fp); // on enlève 7 octet car les 7 premiers octets ne sont pas utiles
			
		}
		nbre_octets +=n-6;
		bzero(buffer,sizeof(buffer)); // vide le buffer
		n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &servaddr, &len);
		
		
	}
	gettimeofday(&time_after,NULL);
	long double time_seconds = (time_after.tv_sec - time_before.tv_sec);
	long double time_microsecondes = abs(time_after.tv_usec - time_before.tv_usec);
	//printf("nombre octets : %d\n",nbre_octets);
	//printf("Durée échange fichier : %Lf secondes et %LF microsecondes\n",time_seconds,time_microsecondes);
	printf("Débit : %Lf octets/s\n",(long double)(nbre_octets/(time_microsecondes/(1000000))));
	printf("%d octets reçu\n",nbre_octets);
	printf("Fichier reçu et crée\n");
	fclose(fp);
}



int main(int argc, char **argv) {
	char *input_file;
	char *output_file;


	if(argc >= 3){
		input_file = argv[1];
		output_file = argv[2];
	}else{
		input_file = "bigfile.txt";
		output_file = "output_file.txt";
	}

	// les arguments du client sont le nom du fichier de sortie et le fichier d'entrée
	int sockfd; // socket du client
	struct sockaddr_in servaddr; // structure qui va contenir notre socket 
    createsocket(&sockfd,&servaddr,PORT);

    int res_twh = thw_client(sockfd,servaddr);
    if(res_twh != 0){
		struct sockaddr_in servaddr_data; // structure qui va contenir notre socket 
		init_struct(&servaddr_data, res_twh);
        exchange_file(sockfd, servaddr_data,input_file,output_file);
		//exchange_data(res_twh,sockfd,servaddr);
    }
	close(sockfd);
	
	char buffer[100];
	snprintf(buffer, sizeof(buffer), "cmp --silent %s %s || echo files are different ", output_file, input_file);
	system(buffer);
	return 0;
}

// à faire : changer le nom des variables, définir send et recv pour les messages d'erreurs;