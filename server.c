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

// le numéro de port public est en argument
#define PORT	 8080
#define PORT2	 1234
#define MAXLINE 1024
#define WINDOW 5 // taille de la fenêtre
#define SEGMENT_SIZE 1024 // taille d'un segment
#define NUMSEQ_SIZE 6 // taille d'un segment

// Rappel cours : 
// fenêtre de congestion => fenêtre qui contient les segments qu'on doit envoyer 
// Flight size : nombre de segments qu'on a envoyé et qui n'ont pas encore été acquitté
//serveur démarre un timer dès qu'il envoie un segment

double alpha = 0.9; // représente le poids qu'on donne à l'historique 
// si notre canal varie beacoup alors il vaut mieux mettre un alpha faible
// s'il varie peu alors il faut mettre un grand alpha 

double timer = 0; // va contenir le timer pour  le rtt
int flightsize = 0;
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
    struct timeval first_ACK_recv_time; 
    struct timeval rtt; 
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

// RTT ne marche que sur un vrai réseau entre 2 machines 
// Un test en local donnera des temps inférieurs à la milliseconde
// et gettimeofday ne descend pas en dessous de la milliseconde.
// Pas la peine de penser aux nanosecondes


// Version récursive : NON pas besoin
// struct timeval * mul(int value, struct timeval *rtt, struct timeval *tv){
//     tv->tv_sec = rtt->tv_sec*value;
//     tv->tv_usec = rtt->tv_usec*value;
//     if(tv->tv_usec>1000){
//         tv->tv_sec += tv->tv_usec/1000;
//         tv->tv_usec = tv->tv_usec%1000;
//     }
//     return tv;
// }


// struct timeval *calcul_rtt(struct segment segments[], int seq, struct timeval *tv){
//     printf("seq n°%d\n",seq);
//     if((seq-1)==1){ // si on est au premier segment
//         //printf("seq == 1 n°%d\n",seq);
//         tv = mul((1-alpha),&(segments[0].rtt),tv);
//     }else{
//         timeradd(mul(alpha,calcul_rtt(segments,seq-1,tv),tv),mul((1-alpha),&(segments[seq-2].rtt),tv),tv);
//     }
//     return tv;
// }

double calcul_rtt(struct segment segments[], int seq){
    // calcule le rtt d'un segment
    // printf(" Trans : %f secondes et  %f millisecondes  \n",(double)segments[seq-1].trans_time.tv_sec,(double)segments[seq-1].trans_time.tv_usec);
    // printf(" Recv : %f secondes et  %f millisecondes  \n",(double)segments[seq-1].first_ACK_recv_time.tv_sec,(double)segments[seq-1].first_ACK_recv_time.tv_usec);
    // return ((segments[seq-1].first_ACK_recv_time.tv_sec*1000+segments[seq-1].first_ACK_recv_time.tv_usec)-(segments[seq-1].trans_time.tv_sec*1000+segments[seq-1].trans_time.tv_usec));
    double secondes = segments[seq-1].first_ACK_recv_time.tv_sec - segments[seq-1].trans_time.tv_sec;
    double millisecondes = abs(segments[seq-1].first_ACK_recv_time.tv_usec - segments[seq-1].trans_time.tv_usec);
    return (secondes*1000+millisecondes);
}

double maj_timer(struct segment segments[], int seq){
    timer = alpha*timer+(1-alpha)*calcul_rtt(segments,seq);
}

void wait_for_ACK(int data_socket, struct sockaddr_in data_addr, int len, int *window_size, int nbre_seg, int *max_ack, struct segment segments[], int *seq, FILE *fp, char buffer_ack[], char message[], char ack_char[]){
    int retval;
    int timer_set = 0;
    fd_set read_fds;
    FD_ZERO(&read_fds); // création du set qui va contenir la socket
    FD_SET(data_socket, &read_fds); // met la socket de données dans le set
    struct timeval tv; // le timer pour un ACK
    if(*window_size==0 || *seq==(int)nbre_seg+1){ // si notre fenêtre est vide ou qu'on a envoyé ts les segments on attend à l'infini   
        // si la fenêtre est à 0 alors on attend un ACK après un timer 
        timer_set = 1;
        // tv.tv_sec = 5; // le nombre de secondes qu'on va attendre, va être en fonction du RTT (pour l'instant 5 sec)
        // tv.tv_usec = 0; // ... millisecondes ...
        tv.tv_sec = 0; // le nombre de secondes qu'on va attendre, va être en fonction du RTT (pour l'instant 5 sec)
        tv.tv_usec = 1.5*timer; // ... millisecondes .., on met 2*le RTT car il y a de la latence
        printf("%d millisecondes d'attente \n",(int)tv.tv_usec);
    }
    else{
        tv = (struct timeval){0}; 
        // initialisé à 0, on n'attend pas, on regarde et sinon on passe au message 
    }
    // il faut démarrer un timer pour chaque message envoyé
    retval = select(data_socket+1, &read_fds, NULL, NULL, &tv); // regarde si la socket a reçu des données pendant tv secondes 
    if (retval == -1)
        perror("select()");
    else if (retval){
        if(FD_ISSET(data_socket, &read_fds)!=0){
            // char buffer_ack[10]; // + 4 car on a "ACK_" au début 
            //bzero(buffer_ack,sizeof(buffer_ack));
            memset( buffer_ack, '\0', sizeof(char)*10);
            memset( message, '\0', sizeof(char)*4);
            memset( ack_char, '\0', sizeof(char)*7);
            //read(data_socket,buffer_ack,sizeof(buffer_ack)); // le message d'ack est de la forme ACK_AAAAAA où AAAAAA est le numéro de l'ACK
            recvfrom(data_socket, (char *)buffer_ack, 10, MSG_WAITALL, ( struct sockaddr *) &data_addr, &len);
            // printf("recv : %d\n",recv);
            // char message[4];
            memcpy(message,buffer_ack,3);
            // char ack_char[7];
            memcpy(ack_char,buffer_ack+3,6);
            if(strcmp(message,"ACK")==0){
                int nv_ack = atoi(ack_char);
                printf("Client : ACK n°%d\n", nv_ack);
                // on va calculer le RTT du segment
                segments[nv_ack-1].nbr_ACK_recv += 1;  // l'ack est le numéro de séquence du message acquitté
                if(segments[nv_ack-1].nbr_ACK_recv==1){ // si c'est le premier ACK reçu 
                    gettimeofday(&segments[nv_ack-1].first_ACK_recv_time, NULL);
                    //timersub(&segments[nv_ack-1].trans_time, &segments[nv_ack-1].last_ACK_recv_time,&segments[nv_ack-1].rtt);
                    // des qu'on reçoit un ACK on met à jour le RTT
                    maj_timer(segments,nv_ack);
                    flightsize--;
                }else{
                    gettimeofday(&segments[nv_ack-1].last_ACK_recv_time, NULL);
                }
                if(nv_ack>*max_ack){
                    *window_size += nv_ack-*max_ack; //((nv_ack-max_ack)+window_size>WINDOW) ? 5 : 
                    *max_ack = nv_ack;
                    // printf("Taille de fenêtre : %d\n",*window_size);
                }else{
                    // si pas dans l'ordre, donc inférieur au plus grand reçu => pas de pb 
                    // si duplicate alors => retransmission 
                    if(segments[nv_ack-1].nbr_ACK_recv >= 3){ // si on a reçu 3 fois ou plus le même ACK
                        *seq = nv_ack+1; // on décale la fenêtre au segment suivant l'ACK dupliqué 
                        printf("DUPLICATE ACK retransmission à partir de : %d\n",*seq);
                        *window_size = WINDOW;      
                        // fseek(fp,(*seq-1)*(SEGMENT_SIZE-NUMSEQ_SIZE),SEEK_SET); // on se place au bon endroit dans le fichier
                    }
                }
            }
        }
    }
    else{
        if(timer_set==1){ // si on a mis un timer et qu'on a pas reçu d'ACK      
            printf("TIMEOUT : Aucun ACK reçu, retransmission à partir de %d\n",*max_ack+1); // le timer a expiré, on va retransmettre à partir du segment suivant le dernier segment acquitté + 1 
            *seq = *max_ack+1;
            *window_size = WINDOW; 
        }else{
            printf("Aucun ACK reçu, message suivant\n");
        }
    }
    printf("Nbres de messages envoyé pas encore acquitté : %d\n",flightsize);
    timer_set = 0;   
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
    //char *seq_char = (char*) malloc(SEGMENT_SIZE*sizeof(char));
    char seq_char[SEGMENT_SIZE];
    //int retval;
    // sprintf(seq_char,"%0*d",NUMSEQ_SIZE ,seq); // on veut écrire le numéro de séquence dans une chaîne de caractères sur NUMSEQ_SIZE chiffres 
    //while( fgets(str, 64, fp)!=NULL ) { // fgets : str => la chaîne qui contient la chaine de caractère lu, 60 : nmbre max de caractère à lire, fr : stream d'où sont lus les chaînes
    int res_read; 
    int max_ack = 0; // on va regarder quel est le plus gd dernier ack recu pour savoir lesquels on a déjà reçu et qu'on ne doit donc pas traité
    int nv_ack;
    struct stat st;
    stat(buffer, &st);
    size_t file_size = st.st_size;
    printf("taille du fichier : %d\n",(int)file_size);


    double nbre_seg = ceil((double)file_size/(double)(SEGMENT_SIZE-NUMSEQ_SIZE)); // on arrondi à l'entier supérieur pour connaître le nombre de segments nécessaires
    printf("%d segments nécessaires\n",(int)nbre_seg);
    if(nbre_seg>640000){
        printf("fichier trop grand \n");
        exit(0);
    }
    // char buffer_ack[(int)nbre_seg];
    // bzero(buffer_ack,sizeof(buffer_ack));
    struct segment segments[(int)nbre_seg];
    // version avec fenêtre
    res_read = fread(str,1,sizeof(str),fp);
    int nbre_octets = res_read;
    char buffer_ack[10]; // 4 + 6 car on a "ACK" au début
    char message[4];  
    // memcpy(message,buffer_ack,3);
    char ack_char[7];
    // buffers utilisé pour recevoir les ACK 
    // memcpy(ack_char,buffer_ack+3,6);
    // while(res_read){
    while(max_ack!=(int)nbre_seg){
        if(seq!=(int)nbre_seg+1){ // si on est pas au dernier segment
            // printf("%d octets lus\n",res_read);
            sprintf(seq_char,"%06d" ,seq);
            memcpy(seq_char+6,str,sizeof(str));
            //puts(str);
            sendto(data_socket, (const char *)seq_char, res_read+NUMSEQ_SIZE, MSG_CONFIRM, (const struct sockaddr *) &data_addr, sizeof(data_addr));
            printf("Message n°%d envoyé au client\n",seq);
            if(segments[seq-1].init!=1){ // si c'est bien la première transmission du segment courant
                segments[seq-1].init = 1;
                gettimeofday(&(segments[seq-1].trans_time),NULL);
                segments[seq-1].byte_length = res_read;
                segments[seq-1].nbr_ACK_recv = 0;
                flightsize++;
            }
            seq++; 
            window_size --; 
        }
        wait_for_ACK(data_socket,data_addr,len,&window_size,nbre_seg, &max_ack, segments,&seq,fp,buffer_ack, message, ack_char);
        printf("seq : %d\n",seq);
        fseek(fp,(seq-1)*(SEGMENT_SIZE-NUMSEQ_SIZE),SEEK_SET); // on se place au bon endroit dans le fichier
        res_read = fread(str,1,sizeof(str),fp); 
        nbre_octets+= res_read;
        if(window_size>1)
            printf("Fenêtre : [%d,%d]\n",seq,seq+window_size);
        else if(window_size==1)
            printf("Fenêtre : [%d]\n",seq);
        else
            printf("Fenêtre : []\n"); // DEBUG : la fenêtre ne peut jamais être à 0 normalement
    }
    char eof[4] = "FIN";
    sendto(data_socket, (const char *)eof, strlen(eof), MSG_CONFIRM, (const struct sockaddr *) &data_addr, sizeof(data_addr));
    fclose(fp);
    close(data_socket);
    printf("fichier de %d octets\n",(int)file_size);
    printf("%d octets lus\n",(int)nbre_octets);
    return 1;
}


int twh_serv(int sockfd, struct sockaddr_in cliaddr){
    //printf("nv twh\n");
    int len, n;
    char buffer[4];
	len = sizeof(cliaddr);//taille de la structure qui mène vers le client
    n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len);
	buffer[n] = '\0';
	printf("Client : %s\n", buffer);
    if((strcmp(buffer,"SYN")) == 0){ //si le client s'est bien connecté alors on crée la socket de données
        memset( buffer, '\0', sizeof(char)*sizeof(buffer));
        int data_socket; //on va créer la socket de donnée
        struct sockaddr_in data_addr;
        //ici on va faire un fork pour gérer plusieurs clients en même temps 
        int port2 = PORT2;
        createsocket(&data_socket,&data_addr,port2);
        
        char message[12] = "SYN-ACK";
        char port[5];
        sprintf(port, "%d", port2);
        strcat(message,port);
        puts(message);
        // message[12] = '\0';
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

int main(int argc, char **argv) {

    int public_port;

    if(argc >= 2){
		public_port = atoi(argv[1]);
	}else{
		public_port = PORT;
	}

	int sockfd;
	struct sockaddr_in servaddr, cliaddr;
	memset(&cliaddr, 0, sizeof(cliaddr));
    createsocket(&sockfd,&servaddr,public_port);

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
