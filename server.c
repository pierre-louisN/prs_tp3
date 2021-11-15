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
#include <sys/stat.h> // pour connaître la taille du fichier
#include <math.h> // pour arrondir
#include <sys/time.h> //gettimeofday


#define PORT	 8080
#define PORT2	 1234
#define MAXLINE 1024
#define WINDOW 5 // taille de la fenêtre
#define SEGMENT_SIZE 1024 // taille d'un segment
#define NUMSEQ_SIZE 6 // taille d'un segment

// on va utiiliser une structure segment qui contient : 
// - les donnes lu du fichier : le point  de départ dans le fichier et le nombre d'octet lus
// - son numéro de séquence 
// - la valeur du temps au moment où on a transmis le segment (va être utile pour déterminer le timer des messages suivants)
// - le nombre d'ACK reçu pour ce segment
// - la valeur du temps au moment où on a reçu le dernier ACK (va être utile pour calculer le RTT et fixer le timer)

struct segment
{
    // int seq_number; // le numéro de séquence va être l'indice dans le tableau, attention les numéros de séquence commence à 1
    int init; // 1 si le segment a été transmis
    int byte_length; 
    struct timeval trans_time;
    int nbr_ACK_recv; 
    struct timeval last_ACK_recv_time; 
};
// le point de départ du segment = seq_number*sizeof(str)-1

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
	
	// Bind de la socket : association de la socket avec la structure qui contient l'adresse et le port du serveur 
    // pour que la socket soit associé à la couche réseau 
	if ( bind(*sockfd, (const struct sockaddr *)servaddr, sizeof(*servaddr)) < 0 )
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
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



int exchange_file(int data_socket, struct sockaddr_in data_addr){
    char buffer[SEGMENT_SIZE];
    bzero(buffer,sizeof(buffer));
    
    int n, len;
    len = sizeof(data_addr);
    // entrée dans la connexion 
    n = recvfrom(data_socket, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &data_addr, &len);
    buffer[n] = '\0'; 
    //printf("ici\n");
    printf("Client sent filename : %s\n", buffer);
    FILE *fp;
    char str[SEGMENT_SIZE-NUMSEQ_SIZE]; // va contenir ce qu'on lit dans le fichier
    /* opening file for reading */
    fp = fopen(buffer , "rb"); // ouverture du fichier
    if(fp == NULL) {
        perror("Error opening file");
        return(-1);
    }
    int seq = 1; // on va écrire le numéro de séquence sur 6 bits
    int window_size = WINDOW; // taille de la fenêtre, on va la décaler dès qu'on reçoit un ACK
    //char seq_char[64];
    char *seq_char = (char*) malloc(SEGMENT_SIZE*sizeof(char));
    int retval;
    sprintf(seq_char,"%0*d",NUMSEQ_SIZE ,seq); // on veut écrire le numéro de séquence dans une chaîne de caractères sur 6 chiffres 
    //while( fgets(str, 64, fp)!=NULL ) { // fgets : str => la chaîne qui contient la chaine de caractère lu, 60 : nmbre max de caractère à lire, fr : stream d'où sont lus les chaînes
    int res_read; 
    int max_ack = 0; // on va regarder quel est le plus gd dernier ack recu pour savoir lesquels on a déjà reçu et qu'on ne doit donc pas traité
    int nv_ack;
    struct stat st;
    stat(buffer, &st);
    size_t file_size = st.st_size;
    printf("taille du fichier : %d\n",(int)file_size);


    double nbre_seg = ceil((double)file_size/(double)(SEGMENT_SIZE-6))+1; // on arrondi à l'entier supérieur pour connaître le nombre de segments nécessaires
    printf("%d segments nécessaires\n",(int)nbre_seg);
    if(nbre_seg>640000){
        printf("fichier trop grand \n");
        exit(0);
    }
    char buffer_ack[(int)nbre_seg];
    bzero(buffer_ack,sizeof(buffer_ack));
    printf("ici\n");
    struct segment segments[(int)nbre_seg];
    // version avec fenêtre
    res_read = fread(str,1,sizeof(str),fp);
    int nbre_octets = res_read;
    
    while(res_read){
        //printf("buffer nul\n");
        //printf("%d octets lus\n",res_read);
        nbre_octets+= res_read;
        //str[res_read] = '\0';
        sprintf(seq_char,"%06d",seq);
        // strcat et strlen ne marche qu'avec des fichier textes, à la place il faut utiliser memcpy et res_read
        //seq_char = concat(seq_char,"_");
        seq_char = concat(seq_char,str);
        //puts(seq_char);
        // on ne peut pas mettre strlen(seq_char) car cela ne marche que pour les string
        // les 7 octets en plus correspondent à "numdeseq_"
        sendto(data_socket, (const char *)seq_char, res_read+NUMSEQ_SIZE, MSG_CONFIRM, (const struct sockaddr *) &data_addr, len);
        printf("Message n°%d envoyé au client\n",seq);
        if(segments[seq].init!=1){ // si c'est bien la première transmission 
            segments[seq].init = 1;
            gettimeofday(&(segments[seq].trans_time),NULL);
            segments[seq].byte_length = res_read;
            segments[seq].nbr_ACK_recv = 0;
        }
        seq++; 
        window_size --;
        

        fd_set read_fds;
        FD_ZERO(&read_fds); // création du set qui va contenir la socket
        FD_SET(data_socket, &read_fds); // met la socket de données dans le set
        struct timeval tv; // le timer pour un ACK
        if(window_size==0){
            // si la fenêtre est à 0 alors on attend un ACK après un timer 
            tv.tv_sec = 1; // le nombre de secondes qu'on va attendre, va être en fonction du RTT 
            tv.tv_usec = 0; // ... millisecondes ...
            printf("fenêtre à O, on attend un ACK au maximum %d secondes sinon on retransmet\n",(int)tv.tv_sec);
             
        }else if(window_size<0){
            printf("fenêtre négative, DUPLICATE ACK (à faire) \n");
        }
        else{
            tv = (struct timeval){0}; 
            // initialisé à 0, on n'attend pas, on regarde et sinon on passe au message 
        }
        
        retval = select(data_socket+1, &read_fds, NULL, NULL, &tv); // regarde si la socket a reçu des données 
        if (retval == -1)
            perror("select()");
        else if (retval){
            if(FD_ISSET(data_socket, &read_fds)!=0){
                int recv = read(data_socket,buffer_ack,sizeof(buffer_ack)); // le message d'ack est de la forme ACK_ + ...AAAA
                //puts(buffer_ack);
                char delim[] = "_";
                char *seq_ptr = strtok(buffer_ack, delim);
                //printf("seq_ptr : %s\n",seq_ptr);
                if(strcmp(seq_ptr,"ACK")==0){
                    //printf("ACK reçu \n");
                    seq_ptr = strtok(NULL, delim);
                    //puts(seq_ptr);
                    int nv_ack = strlen(seq_ptr);
                    // buffer[recv] = '\0'; 
                    // nv_ack = atoi(buffer);
                    segments[nv_ack].nbr_ACK_recv += 1;  // l'ack est le numéro de séquence du message acquitté
                    gettimeofday(&(segments[nv_ack].last_ACK_recv_time),NULL);
                    printf("Client : ACK n°%d\n", nv_ack);
                    if(nv_ack>max_ack){
                        window_size += nv_ack-max_ack; //((nv_ack-max_ack)+window_size>WINDOW) ? 5 : 
                        max_ack = nv_ack;
                        printf("Taille de fenêtre : %d\n",window_size);
                    }else{
                        //printf("DUPLICATE ACK ou OUT OF ORDER\n"); // on reçoit un ACK qu'on a déjà où alors pas dans l'ordre
                        // si pas dans l'ordre, donc inférieur au plus grand reçu => pas de pb 
                        // si duplicate alors => retransmission 
                        if(segments[nv_ack].nbr_ACK_recv >= 3){ // si on a reçu 3 fois ou plus le même ACK
                            printf("Retransmission à faire (duplicate ACK) \n");
                            seq -= segments[nv_ack].nbr_ACK_recv;
                            fseek(fp,(seq-1)*(SEGMENT_SIZE-NUMSEQ_SIZE),SEEK_SET);
                            // pour retransmettre les segments, on va décaler la fenêtre
                        }
                    }
                }
                
        
            }
        }
        else{
            printf("Aucun ACK reçu.\n");
        }

        if(window_size>1)
            printf("Fenêtre : [%d,%d]\n",seq,seq+window_size);
        else if(window_size==1)
            printf("Fenêtre : [%d]\n",seq);
        else
            printf("Fenêtre : []\n");
        res_read = fread(str,1,sizeof(str),fp);
    }
    char *eof = "EOF";
    sendto(data_socket, (const char *)eof, strlen(eof), MSG_CONFIRM, (const struct sockaddr *) &data_addr, len);
    fclose(fp);
    close(data_socket);
    printf("fichier de %d octets\n",(int)file_size);
    printf("%d octets lus\n",(int)nbre_octets);
    printf("%d : dernier ACK\n",max_ack);
    return 1;

}


int twh_serv(int sockfd, struct sockaddr_in cliaddr){
    //printf("nv twh\n");
    int len, n;
    char buffer[MAXLINE];
	len = sizeof(cliaddr);//taille de la structure qui mène vers le client
    n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len);
	buffer[n] = '\0';
	//printf("Client : %s\n", buffer);
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
        // le fork est fait quand on fait la socket de données, le fils gère les données et le père acceptre les clients    
    }
	return 0;
}
