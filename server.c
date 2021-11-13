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


#define PORT	 8080
#define PORT2	 1234
#define MAXLINE 1024
#define WINDOW 5 // taille de la fenêtre
#define SEGMENT_SIZE 64 // taille de la fenêtre

// on va utiiliser une structure segment qui contient : 
// - les donnes lu du fichier : le point  de départ dans le fichier et le nombre d'octet lus
// - son numéro de séquence 
// - la valeur du temps au moment où on a transmis le segment (va être utile pour déterminer le timer des messages suivants)
// - le nombre d'ACK reçu pour ce segment
// - la valeur du temps au moment où on a reçu le dernier ACK (va être utile pour calculer le RTT et fixer le timer)

struct segment
{
    int seq_number;
    int byte_length; 
    struct timeval trans_time;
    int ACK_received; 
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
    char buffer[MAXLINE];
    char buffer_ack[MAXLINE];
    int n, len;
    len = sizeof(data_addr);
    // entrée dans la connexion 
    n = recvfrom(data_socket, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &data_addr, &len);
    buffer[n] = '\0'; 
    //printf("ici\n");
    printf("Client sent filename : %s\n", buffer);
    FILE *fp;
    char str[SEGMENT_SIZE-6]; // va contenir ce qu'on lit dans le fichier
    /* opening file for reading */
    fp = fopen(buffer , "r"); // ouverture du fichier
    if(fp == NULL) {
        perror("Error opening file");
        return(-1);
    }
    int seq = 1; // on va écrire le numéro de séquence sur 6 bits
    int window_size = WINDOW; // taille de la fenêtre, on va la décaler dès qu'on reçoit un ACK
    //char seq_char[64];
    char *seq_char = (char*) malloc(SEGMENT_SIZE*sizeof(char));
    int retval;
    sprintf(seq_char,"%06d",seq); // on veut écrire le numéro de séquence dans une chaîne de caractères sur 6 chiffres 
    //while( fgets(str, 64, fp)!=NULL ) { // fgets : str => la chaîne qui contient la chaine de caractère lu, 60 : nmbre max de caractère à lire, fr : stream d'où sont lus les chaînes
    int res_read; 
    int max_ack = 0; // on va regarder quel est le plus gd dernier ack recu pour savoir lesquels on a déjà reçu et qu'on ne doit donc pas traité
    int nv_ack;
    struct stat st;
    stat(buffer, &st);
    size_t file_size = st.st_size;
    printf("taille du fichier : %d\n",(int)file_size);


    double nbre_seg = ceil((double)file_size/(double)(SEGMENT_SIZE-6))+1; // on arrondi à l'entier supérieur pour connaître le nombre de segments nécessaires
    //int nbre_seg = ceil((double)17.7);
    printf("%d segments nécessaires\n",(int)nbre_seg);
    struct segment segments[(int)nbre_seg];
    // version avec fenêtre
    res_read = fread(str,1,sizeof(str),fp);
    int nbre_octets = res_read;
    while(res_read){
        //printf("buffer nul\n");
        printf("%d octets lus\n",res_read);
        nbre_octets+= res_read;
        //str[res_read] = '\0';
        sprintf(seq_char,"%06d",seq);
        // strcat et strlen ne marche qu'avec des fichier textes, à la place il faut utiliser memcpy et res_read
        //seq_char = concat(seq_char,"_");
        seq_char = concat(seq_char,str);
        puts(seq_char);
        // on ne peut pas mettre strlen(seq_char) car cela ne marche que pour les string
        // les 7 octets en plus correspondent à "numdeseq_"
        sendto(data_socket, (const char *)seq_char, res_read+6, MSG_CONFIRM, (const struct sockaddr *) &data_addr, len);
        printf("Message n°%d envoyé au client\n",seq);
        seq++; 
        window_size --;
        

        fd_set read_fds;
        FD_ZERO(&read_fds); // création du set qui va contenir la socket
        FD_SET(data_socket, &read_fds); // met la socket de données dans le set
        struct timeval tv; // le timer pour un ACK
        if(window_size==0){
            // si la fenêtre est à 0 alors on attend un ACK après un timer 
            tv.tv_sec = 1; // le nombre de secondes qu'on va attendre 
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
                
                    printf("ACK n°%d reçu du client\n", nv_ack);
                    if(nv_ack>max_ack){
                        window_size += nv_ack-max_ack; //((nv_ack-max_ack)+window_size>WINDOW) ? 5 : 
                        max_ack = nv_ack;
                        printf("Taille de fenêtre : %d\n",window_size);
                    }else{
                        printf("DUPLICATE ACK ou OUT OF ORDER\n");
                        // if(tv.tv_sec!=0){
                        //     //return 1;
                        //     printf("Retransmission à faire (duplicate ACK) \n");
                        // }
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
            // Test fork : 
            // int child_pid = fork();
            // if (child_pid == 0){
            //     printf("Hello from Child!\n");
            //     close(sockfd);
            //     int ex = exchange_file(data_socket,data_addr); 
            //     if(ex==1){
            //         // printf("exh succes\n");
            //         // return child_pid; // le fils return son pid pour que son père le tue
            //         exit( EXIT_SUCCESS );
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
        // le fork est fait quand on fait la socket de données, le fils gère les données et le père acceptre les clients    
    }
	return 0;
}
