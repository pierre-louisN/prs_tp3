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
#include <signal.h>

#define PORT	 8080
#define PORT2	 1234
#define MAXLINE 1024

int nbr_sock = 0;

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

int exchange_file(int data_socket, struct sockaddr_in data_addr){
    char buffer[MAXLINE];
    int n, len;
    len = sizeof(data_addr);
    // entrée dans la connexion 
    n = recvfrom(data_socket, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &data_addr, &len);
    buffer[n] = '\0'; 
    //printf("ici\n");
    printf("Client sent filename : %s\n", buffer);
    FILE *fp;
    char str[57];

    /* opening file for reading */
    fp = fopen(buffer , "r"); // ouverture du fichier
    if(fp == NULL) {
        perror("Error opening file");
        return(-1);
    }
    int seq = 1; // on va écrire le numéro de séquence sur 6 bits
    char seq_char[64];
    sprintf(seq_char,"%06d",seq); // on veut écrire le numéro de séquence dans une chaîne de caractères sur 6 chiffres 
    //while( fgets(str, 64, fp)!=NULL ) { // fgets : str => la chaîne qui contient la chaine de caractère lu, 60 : nmbre max de caractère à lire, fr : stream d'où sont lus les chaînes
    int read = fread(str,1,sizeof(str)-1,fp);
    while(read==sizeof(str)-1){ // tant qu'on arrive à lire les 56 octets
    
        /* writing content to stdout */
        str[read] = '\0';
        //puts(str);
        // on va écrire le texte vers le client 
        sprintf(seq_char,"%6d",seq);
        strcat(seq_char,"_");
        strcat(seq_char,str);
        //printf("seq : %s\n",seq_char);
        sendto(data_socket, (const char *)seq_char, strlen(seq_char), MSG_CONFIRM, (const struct sockaddr *) &data_addr, len);
        //bzero(str,sizeof(str));
        n = recvfrom(data_socket, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &data_addr, &len);
        buffer[n] = '\0'; 
        printf("Client : %s\n", buffer);
        seq++;
        bzero(str,sizeof(str));
        read = fread(str,1,sizeof(str)-1,fp);
        //printf("read : %d\n",read);
        //puts(str);
    }
    str[read] = '\0';
    sprintf(seq_char,"%6d",seq);
    strcat(seq_char,"_");
    char last_message[read];
    strcpy(last_message,str);
    
    
    strcat(seq_char,last_message);
    printf("Serveur (dernier message) : %s\n",seq_char);
    sendto(data_socket, (const char *)seq_char, strlen(seq_char), MSG_CONFIRM, (const struct sockaddr *) &data_addr, len);
    //bzero(str,sizeof(str));
    n = recvfrom(data_socket, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &data_addr, &len);
    buffer[n] = '\0'; 
    printf("Client : %s\n", buffer);
    //puts(str);
    strcpy(str,"EOF");
    sendto(data_socket, (const char *)str, strlen(str), MSG_CONFIRM, (const struct sockaddr *) &data_addr, len);
    fclose(fp);
    return 1;
}


void handle_client(int data_socket, struct sockaddr_in data_addr){
    char buffer[MAXLINE];
    bzero(buffer,sizeof(buffer));
    int len = sizeof(data_addr);
    while(1){
        int n = recvfrom(data_socket, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &data_addr, &len);
        buffer[n] = '\0'; 
	    printf("Client : %s\n", buffer);
        char message[64] = "ACK_"; // on va concaténer l'ACK avec le numéro du dernier message reçu en continu
        // on suppose que le message est son propre numéro
        // char port[5];
        // sprintf(port, "%d", port2);
        strcat(message,buffer);
        printf("message : %s\n", message);
        sendto(data_socket, (const char *)message, strlen(message), MSG_CONFIRM, (const struct sockaddr *) &data_addr, len);
    }
}


int twh_serv(int sockfd, struct sockaddr_in cliaddr){
    printf("nv twh\n");
    int len, n;
    char buffer[MAXLINE];
	len = sizeof(cliaddr);//taille de la structure qui mène vers le client
    n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len);
	buffer[n] = '\0';
	printf("Client : %s\n", buffer);
    if((strcmp(buffer,"SYN")) == 0){ //si le client s'est bien connecté alors on crée la socket de données
        
        int data_socket; //on va créer la socket de donnée
        struct sockaddr_in data_addr;
        //ici on va faire un fork pour gérer plusieurs clients en même temps 
        int port2 = PORT2;
        printf("port 2 : %d\n",port2);
        createsocket(&data_socket,&data_addr,port2);
        
        char message[64] = "SYN-ACK_";
        char port[5];
        sprintf(port, "%d", port2);
        strcat(message,port);
        sendto(sockfd, (const char *)message, strlen(message), MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);
        
        n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len);
        buffer[n] = '\0'; 
	    printf("Client : %s\n", buffer);
        if(strcmp(buffer,"ACK") == 0){
            int ex = exchange_file(data_socket,data_addr); 
            if(ex==1){
                return 1; 
            }
            //nbr_sock ++;
            // int child_pid = fork();
            // if (child_pid == 0){
            //     printf("Hello from Child!\n");
            //     close(sockfd);
            //     int ex = exchange_file(data_socket,data_addr); 
            //     if(ex==1){
            //         return child_pid; // le fils return son pid pour que son père le tue
            //     }
            //     //handle_client(data_socket,data_addr);   
            // }
            // // parent process because return value non-zero.
            // else{
            //     printf("Hello from Parent!\n");
            //     close(data_socket);
            //     return 1;
            // }
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

    while(1){
        int res_twh = twh_serv(sockfd,cliaddr); // twh correspond au connect en TCP
        //printf(res_twh);
        if(res_twh == 1){
            printf("twh successful \n");
        }
        else if(res_twh == 0){
            printf("error, twh unsuccessful\n");
            //exit(EXIT_FAILURE);
        }
        else{
            printf("file exchange successful \n");
            kill(res_twh,SIGTERM); // le père tue le fils après qu'il ait finit de faire l'échange de fichier
        }
        // le fork est fait quand on fait la socket de données, le fils gère les données et le père acceptre les clients    
    }
	return 0;
}
