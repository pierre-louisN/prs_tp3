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
#define MAXLINE 1500
#define WINDOW 5 // taille initiale de la fenêtre (quand on a une perte on va revenir à cette valeur)
#define SEGMENT_SIZE 1500 // taille d'un segment
#define NUMSEQ_SIZE 6 // taille d'un segmente valeur)

int cwnd = 1; // mettre ça dans une fonction, pas en local 
int window_size;
int timer_set = 0;
int max_ack = 0;
int flightsize = 0;
int ssthresh = 2147483647;
int nv_ack;
int seq = 1;
int nbre_octets = 0;
int deflate_ack = 0;
int res_read;
// int seq = 1; // on va écrire le numéro de séquence sur 6 bits
// //int window_size = WINDOW; // taille de la fenêtre, on va la décaler dès qu'on reçoit un ACK
// // int cwnd = 1; // on commence avec la fenêtre à 1 => SLOW START
// int window_size = cwnd; // window_size : le nombre de segments qu'on va pouvoir envoyer avant de se bloquer pendant RTT pour recevoir un ACK
// int ssthresh = 2147483647; // va être le seuil pour lequel on passe de SLOW START à CONGESTION AVOIDANCE 
// int flightsize = 0;
// //char seq_char[64];
// //char *seq_char = (char*) malloc(SEGMENT_SIZE*sizeof(char));
// char seq_char[SEGMENT_SIZE];
// //int retval;
// // sprintf(seq_char,"%0*d",NUMSEQ_SIZE ,seq); // on veut écrire le numéro de séquence dans une chaîne de caractères sur NUMSEQ_SIZE chiffres 
// //while( fgets(str, 64, fp)!=NULL ) { // fgets : str => la chaîne qui contient la chaine de caractère lu, 60 : nmbre max de caractère à lire, fr : stream d'où sont lus les chaînes
// int res_read; 
// int max_ack = 0; // on va regarder quel est le plus gd dernier ack recu pour savoir lesquels on a déjà reçu et qu'on ne doit donc pas traité
// int nv_ack;

// paramètres TCP
int slow_start = 1;
int congestion_avoidance = 0;
int fast_retransmit = 1;
int fast_recovery = 1;
int lost_seg = 0; // nombre de retranssmission car segments perdu
int lost_ack= 0; // nombre de retranssmission car ack perdu 
int retransmission = 0;

// Rappel cours : 
// fenêtre de congestion => fenêtre qui contient les segments qu'on doit envoyer 
// Flight size : nombre de segments qu'on a envoyé et qui n'ont pas encore été acquitté
//serveur démarre un timer dès qu'il envoie un segment

double alpha = 0.9; // représente le poids qu'on donne à l'historique 
// si notre canal varie beacoup alors il vaut mieux mettre un alpha faible
// s'il varie peu alors il faut mettre un grand alpha 

double timer = 0; // le nombre de microsecondes durant lesquels le serveur va attendre un ACK quand sa fenêtre est vide

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
    char bytes[SEGMENT_SIZE];
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

void handle_ACK(struct segment segments[]){
    int i = (nv_ack-max_ack); // le nombre de segments à ACK
    //printf("%d segments à acquitter\n",i);
    while(i!=0){
        segments[(max_ack+i)-1].nbr_ACK_recv += 1;
        if(segments[nv_ack-1].nbr_ACK_recv==1){ // si c'est le premier ACK reçu 
            gettimeofday(&segments[nv_ack-1].first_ACK_recv_time, NULL);
            //timersub(&segments[nv_ack-1].trans_time, &segments[nv_ack-1].last_ACK_recv_time,&segments[nv_ack-1].rtt);
            // des qu'on reçoit un ACK on met à jour le RTT
            maj_timer(segments,nv_ack);
            flightsize--; 
        }else{
            gettimeofday(&segments[nv_ack-1].last_ACK_recv_time, NULL);
        }
        i--;
    }
    printf("Nombre ACK reçu pour segments 0 %d\n",segments[0].nbr_ACK_recv);
    if(window_size>ssthresh && congestion_avoidance){ // CONGESTION AVOIDANCE => cwnd += 1/cwnd pour chaque ACK
        cwnd += (nv_ack-max_ack)/(cwnd);    
    }else{ // SLOW START => cwnd +=1 pour chaque ACK 
        cwnd += nv_ack-max_ack;
    }                    
    
    max_ack = nv_ack;
    // si pas dans l'ordre, donc inférieur au plus grand reçu => pas de pb 
    // si duplicate alors => retransmission 
    if(segments[nv_ack-1].nbr_ACK_recv >= 3){ // si on a reçu 3 fois ou plus le même ACK
        seq = nv_ack+1; // on décale la fenêtre au segment suivant l'ACK dupliqué 
        printf("DUPLICATE ACK retransmission à partir de : %d\n",seq);
        lost_seg ++;
        ssthresh = flightsize/2;
        // *window_size = WINDOW;
        if(fast_retransmit){
            flightsize = cwnd; // on suppose que cwnd est forcément < rwnd, normalement il faut faire min(rwnd,cwnd) mais on ne connaît pas rwnd
            cwnd = 1;
            congestion_avoidance = 0; // on repasse en SLOW START
        }
        if(fast_recovery){
            cwnd=ssthresh+segments[nv_ack-1].nbr_ACK_recv; // inflation de la fenêtre
            congestion_avoidance = 1; // on passe directement en CONGESTION AVOIDANCE
        }else{	
            cwnd = 1;    
            window_size = cwnd;
            congestion_avoidance = 1;   
        }
        if(fast_retransmit && fast_recovery){
            deflate_ack = nv_ack+1; // l'ack qu'on attend pour deflate la fenêtre
        }
        // fseek(fp,(*seq-1)*(SEGMENT_SIZE-NUMSEQ_SIZE),SEEK_SET); // on se place au bon endroit dans le fichier
    }else{
        printf("1er ou 2eme DUPLICATE ACK \n");
        if(fast_retransmit && fast_recovery){
            seq = nv_ack+1;
            window_size =  (cwnd == 0) ? 1 : cwnd;
            // cwnd ne change pas et windows size est à 1 
        }
    }
}


void recv_ACK(int data_socket, struct sockaddr_in data_addr, int len, int nbre_seg,struct segment segments[], FILE *fp, int *retval){
    char buffer_ack[10]; // + 4 car on a "ACK_" au début 
    recvfrom(data_socket, (char *)buffer_ack, 10, MSG_WAITALL, ( struct sockaddr *) &data_addr, &len);
    // printf("recv : %d\n",recv);
    char message[4];
    memcpy(message,buffer_ack,3);
    char ack_char[7];
    memcpy(ack_char,buffer_ack+3,6);
    if(strcmp(message,"ACK")==0){
        nv_ack = atoi(ack_char);
        printf("Client : ACK n°%d\n", nv_ack);
        printf("Dernier ACK reçu : n°%d\n", max_ack);
        if(nv_ack==deflate_ack){ // si on a reçu l'ack suivant l'ack qu'on a reçu en dupliqué 
                printf("DEFLATE ACK reçu\n");
                ssthresh = flightsize/2;
                cwnd=ssthresh; // deflation de la fenêtre
        }
        if(nv_ack>=max_ack){
            handle_ACK(segments);
        
            // On the first and second duplicate ACKs received at a sender, a
            // TCP SHOULD send a segment of previously unsent data per [RFC3042]
            // provided that the receiver's advertised window allows, the total
            // FlightSize would remain less than or equal to cwnd plus 2*SMSS,
            // and that new data is available for transmission.  Further, the
            // TCP sender MUST NOT change cwnd to reflect these two segments
            // [RFC3042].
        }else{ // si on a bien reçu un ACK mais qui est dans le mauvais ordre 
            if(timer_set = 1){ // si notre fenêtre était à 0, alors on considère que le serveur n'a pas reçu d'ACK  
                *retval = 0;
            }
        }
    }
}


void wait_for_ACK(int data_socket, struct sockaddr_in data_addr, int len, int nbre_seg, struct segment segments[], FILE *fp){
    int retval;
    //int timer_set = 0;
    fd_set read_fds;
    FD_ZERO(&read_fds); // création du set qui va contenir la socket
    FD_SET(data_socket, &read_fds); // met la socket de données dans le set
    struct timeval tv; // le timer pour un ACK
    if(window_size==0 || seq==(int)nbre_seg+1){ // si notre fenêtre est vide ou qu'on a envoyé ts les segments on attend à l'infini   
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
        // initialisé à 0, on n'attend pas, on regarde et sinon on passe au segment suivant
    }
    // il faut démarrer un timer pour chaque message envoyé
    retval = select(data_socket+1, &read_fds, NULL, NULL, &tv); // regarde si la socket a reçu des données pendant tv secondes 
    if (retval == -1)
        perror("select()");
    else if (retval){ // on a reçu un ACK 
        if(FD_ISSET(data_socket, &read_fds)!=0){
            printf("ACK reçu \n");
            recv_ACK(data_socket,data_addr,len,nbre_seg,segments,fp,&retval);
        }
    }
    if(!retval){
        if(timer_set==1){ // si on a mis un timer (fenêtre à 0) et qu'on a pas reçu d'ACK ou qu'on a reçu un ACK qui était pour des segments précédents 
            printf("TIMEOUT : Aucun ACK reçu, retransmission à partir de %d\n",max_ack+1); // le timer a expiré, on va retransmettre à partir du segment suivant le dernier segment acquitté + 1 
            seq = max_ack+1;
            ssthresh = flightsize/2;    
            if(ssthresh==0){
                ssthresh = 2; // le seuil ne peut pas être à 0 ou à 1 
            }
            cwnd = 1;
            congestion_avoidance = 1;    
            window_size = cwnd;
            lost_ack ++;
            // *window_size = WINDOW; 
        }else{
            printf("Aucun ACK reçu, message suivant\n");
        }
    }
    //printf("Nbres de messages envoyé pas encore acquitté : %d\n",*flightsize);
    
    printf("Taille de fenêtre : %d\n",window_size);
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
    // int seq = 1; // on va écrire le numéro de séquence sur 6 bits
    // //int window_size = WINDOW; // taille de la fenêtre, on va la décaler dès qu'on reçoit un ACK
    // // int cwnd = 1; // on commence avec la fenêtre à 1 => SLOW START
    // int window_size = cwnd; // window_size : le nombre de segments qu'on va pouvoir envoyer avant de se bloquer pendant RTT pour recevoir un ACK
    // int ssthresh = 2147483647; // va être le seuil pour lequel on passe de SLOW START à CONGESTION AVOIDANCE 
    // int flightsize = 0;
    //char seq_char[64];
    //char *seq_char = (char*) malloc(SEGMENT_SIZE*sizeof(char));
    char seq_char[SEGMENT_SIZE];
    //int retval;
    // sprintf(seq_char,"%0*d",NUMSEQ_SIZE ,seq); // on veut écrire le numéro de séquence dans une chaîne de caractères sur NUMSEQ_SIZE chiffres 
    //while( fgets(str, 64, fp)!=NULL ) { // fgets : str => la chaîne qui contient la chaine de caractère lu, 60 : nmbre max de caractère à lire, fr : stream d'où sont lus les chaînes
     
    // int max_ack = 0; // on va regarder quel est le plus gd dernier ack recu pour savoir lesquels on a déjà reçu et qu'on ne doit donc pas traité
    // int nv_ack;
    struct stat st;
    stat(buffer, &st);
    size_t file_size = st.st_size;
    printf("Taille du fichier : %d\n",(int)file_size);


    double nbre_seg = ceil((double)file_size/(double)(SEGMENT_SIZE-NUMSEQ_SIZE)); // on arrondi à l'entier supérieur pour connaître le nombre de segments nécessaires
    printf("%d segments nécessaires\n",(int)nbre_seg);
    if(nbre_seg>65535){
        printf("fichier trop grand \n");
        exit(0);
    }
    struct segment segments[(int)nbre_seg];
    //struct segment segments[666667];
    // res_read = fread(str,1,sizeof(str),fp);
    // int nbre_octets = res_read;
    // int nbre_octets = 0;
    // int deflate_ack = 0;
    window_size = cwnd;
    while(max_ack!=(int)nbre_seg){ // tant qu'on a pas reçu le dernier ACK
        if(segments[seq-1].init==1){ // si on a déjà transmis on prend les octets stockés en mémoire 
            memcpy(str,segments[seq-1].bytes,segments[seq-1].byte_length);
            //puts(str);
        }else{ // sinon on lit dans le fichier
            res_read = fread(str,1,sizeof(str),fp); 
            nbre_octets+= res_read;
        }        
        if(seq!=(int)nbre_seg+1){ // si on est pas au dernier segment et qu'on a pas reçu l'ACK
            // printf("%d octets lus\n",res_read);
            sprintf(seq_char,"%06d" ,seq);
            memcpy(seq_char+6,str,sizeof(str));
            puts(seq_char);
            sendto(data_socket, (const char *)seq_char, res_read+NUMSEQ_SIZE, MSG_CONFIRM, (const struct sockaddr *) &data_addr, sizeof(data_addr));
            printf("Message n°%d sent\n",seq);
            if(segments[seq-1].init!=1){ //si c'est bien la première transmission du segment courant
                segments[seq-1].init = 1;
                gettimeofday(&(segments[seq-1].trans_time),NULL);
                segments[seq-1].byte_length = res_read;
                // segments[seq-1].bytes = str;
                memcpy(segments[seq-1].bytes,str,res_read);
                segments[seq-1].nbr_ACK_recv = 0;
                flightsize++;
            }else{
                retransmission++;
            }
            seq++; 
            window_size --; 
        }
        wait_for_ACK(data_socket,data_addr,len,nbre_seg,segments,fp);
        //printf("seq : %d\n",seq);
        //fseek(fp,(seq-1)*(SEGMENT_SIZE-NUMSEQ_SIZE),SEEK_SET); // on se place au bon endroit dans le fichier, mais est couteux en temps, donc il faut stocker les bits lus dans le tableau
        // res_read = fread(str,1,sizeof(str),fp); 
        // nbre_octets+= res_read;
        if(window_size>1){
            printf("Fenêtre : [%d,%d]\n",seq,seq+window_size);
        }
        else if(window_size==1){
            printf("Fenêtre : [%d]\n",seq);
        }
        else{
            printf("Fenêtre : []\n"); // DEBUG : la fenêtre ne peut jamais être à 0, on a toujours des messages à envoyer
            // elle est à 0 si on attend un ACK ap
            exit(0);
        }
            
    }
    char eof[4] = "FIN";
    sendto(data_socket, (const char *)eof, strlen(eof), MSG_CONFIRM, (const struct sockaddr *) &data_addr, sizeof(data_addr));
    fclose(fp);
    close(data_socket);
    printf("fichier de %d octets\n",(int)file_size);
    printf("%d octets lus\n",(int)nbre_octets);
    printf("%d segments perdus\n",(int)lost_seg);
    printf("%d ACK perdus\n",(int)lost_ack);
    printf("%d segments retransmis sur %d segment initiaux\n",(int)((nbre_octets-file_size)/SEGMENT_SIZE),(int)nbre_seg);
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
            system("cmp bigfile.txt copy_bigfile.txt");
        }
        // le fork est fait quand on fait la socket de données, le fils gère les données et le père acceptre les clients    
    }
	return 0;
}
