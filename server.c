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
#define WINDOW 10 // taille initiale de la fenêtre (quand on a une perte on va revenir à cette valeur)
#define SEGMENT_SIZE 1500 // taille d'un segment
#define NUMSEQ_SIZE 6 // taille du numéro de séquence
#define BUFFER_SIZE 64000 // taille du buffer circulaire


//Notes importantes : 
// On the first and second duplicate ACKs received at a sender, a
// TCP SHOULD send a segment of previously unsent data per [RFC3042]
// provided that the receiver's advertised window allows, the total
// FlightSize would remain less than or equal to cwnd plus 2*SMSS,
// and that new data is available for transmission.  Further, the
// TCP sender MUST NOT change cwnd to reflect these two segments
// [RFC3042].

// RTT ne marche que sur un vrai réseau entre 2 machines 
// Un test en local donnera des temps inférieurs à la milliseconde
// et gettimeofday ne descend pas en dessous de la milliseconde.
// Pas la peine de penser aux nanosecondes

// Rappel cours : 
// fenêtre de congestion => fenêtre qui contient les segments qu'on doit envoyer 
// Flight size : nombre de segments qu'on a envoyé et qui n'ont pas encore été acquitté
//serveur démarre un timer dès qu'il envoie un segment

// RTO : timer qui décrit le nombre de secondes qu'on va attendre avant de retransmettre
// initialisé à 1 secondes 
// RTO  = 2 * RTO à chaque retransmission 


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
int res_sent = 0;
int nbre_seg = 0;
int buff_size;
// paramètres TCP
int slow_start = 1;
int congestion_avoidance = 0;
int fast_retransmit = 1;
int fast_recovery = 1;
int lost_seg = 0; // nombre de retranssmission car segments perdu
int lost_ack= 0; // nombre de retranssmission car ack perdu 
int retransmission = 0;



double alpha = 0.1; // représente le poids qu'on donne à l'historique 
// si notre canal varie beacoup alors il vaut mieux mettre un alpha faible
// s'il varie peu alors il faut mettre un grand alpha 

double timer = 1000; // le nombre de microsecondes durant lesquels le serveur va attendre un ACK quand sa fenêtre est vide

int rto_enable = 1; 
int rto = 0; 

// on va utiiliser une structure segment qui contient : 
// - les donnes lu du fichier : le point  de départ dans le fichier et le nombre d'octet lus
// - son numéro de séquence 
// - la valeur du temps au moment où on a transmis le segment (va être utile pour déterminer le timer des messages suivants)
// - le nombre d'ACK reçu pour ce segment
// - la valeur du temps au moment où on a reçu le dernier ACK (va être utile pour calculer le RTT et fixer le timer)

struct segment
{
    // int seq_number; // le numéro de séquence va être l'indice dans le tableau, attention les numéros de séquence commence à 1 (en fait non car ça va poser pb pour le buffer circulaire)
    int init; // 1 si le segment a été transmis
    int byte_length; 
    char bytes[SEGMENT_SIZE];
    struct timeval trans_time;
    int nbr_ACK_recv; 
    //struct timeval first_ACK_recv_time; 
    struct timeval rtt; 
    struct timeval last_ACK_recv_time; 
    int seq;
};

void reset_var(int alpha){ // on va reset les variables pour les nouveaux clients 
     cwnd = 1; // mettre ça dans une fonction, pas en local 
     window_size;
     timer_set = 0;
     max_ack = 0;
     flightsize = 0;
     ssthresh = 2147483647;
     nv_ack;
     seq = 1;
     nbre_octets = 0;
     deflate_ack = 0;
     res_read;
     res_sent = 0;
     nbre_seg = 0;
     buff_size;
    // paramètres TCP
     slow_start = 1;
     congestion_avoidance = 0;
     fast_retransmit = 1;
     fast_recovery = 1;
     lost_seg = 0; // nombre de retranssmission car segments perdu
     lost_ack= 0; // nombre de retranssmission car ack perdu 
     retransmission = 0;
    alpha = alpha; // représente le poids qu'on donne à l'historique 

    timer = 1000; // le nombre de microsecondes durant lesquels le serveur va attendre un ACK quand sa fenêtre est vide

    rto_enable = 0; 
    rto = 2*timer; 
}




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

double calcul_rtt(struct segment segments[], int seq){
    // printf("Case n°%d du tableau pour le segment n°%d\n",(seq-1)%buff_size,seq);
    // printf("Trans : %f secondes et  %f millisecondes  \n",(double)segments[(seq-1)%buff_size].trans_time.tv_sec,(double)segments[(seq-1)%buff_size].trans_time.tv_usec);
    // printf("Last Recv : %f secondes et  %f millisecondes  \n",(double)segments[(seq-1)%buff_size].last_ACK_recv_time.tv_sec,(double)segments[(seq-1)%buff_size].last_ACK_recv_time.tv_usec);
    //return ((segments[seq-1].first_ACK_recv_time.tv_sec*1000+segments[seq-1].first_ACK_recv_time.tv_usec)-(segments[seq-1].trans_time.tv_sec*1000+segments[seq-1].trans_time.tv_usec));
    double secondes = segments[(seq-1)%buff_size].last_ACK_recv_time.tv_sec - segments[(seq-1)%buff_size].trans_time.tv_sec;
    double millisecondes = abs(segments[(seq-1)%buff_size].last_ACK_recv_time.tv_usec - segments[(seq-1)%buff_size].trans_time.tv_usec);
    return (secondes*1000000+millisecondes);
}

double maj_timer(struct segment segments[], int seq){
    timer = alpha*timer+(1-alpha)*calcul_rtt(segments,seq);
    if((int)timer<0){
        printf("error, timer négatif \n");
        printf("Case n°%d du tableau pour le segment n°%d\n",(seq-1)%buff_size,seq);
        printf("Trans : %f secondes et  %f millisecondes  \n",(double)segments[(seq-1)%buff_size].trans_time.tv_sec,(double)segments[(seq-1)%buff_size].trans_time.tv_usec);
        printf("Last Recv : %f secondes et  %f millisecondes  \n",(double)segments[(seq-1)%buff_size].last_ACK_recv_time.tv_sec,(double)segments[(seq-1)%buff_size].last_ACK_recv_time.tv_usec);
        exit(0);
    }
    if(rto_enable){
        rto = 2*timer; 
    }
}

void handle_DACK(struct segment segments[]){
    // Le délai de retransmission pour un segment donné est doublé après chaque retransmission de ce segment.
    seq = nv_ack+1; // on décale la fenêtre au segment suivant l'ACK dupliqué 
    //printf("DUPLICATE ACK retransmission à partir de : %d\n",seq);
    ssthresh = flightsize/2;
    // *window_size = WINDOW;
    if(fast_retransmit){
        //printf("FAST RETRANSMIT\n");
        //flightsize = cwnd; // on suppose que cwnd est forcément < rwnd, normalement il faut faire min(rwnd,cwnd) mais on ne connaît pas rwnd
        cwnd = WINDOW;
        window_size = cwnd;
        congestion_avoidance = 0; // on repasse en SLOW START
    }
    if(fast_recovery){
        //printf("FAST RECOVERY\n");
        //printf("Inflation de la fenêtre de : %d avec threshold = %d\n",segments[(nv_ack-1)%buff_size].nbr_ACK_recv,ssthresh);
        cwnd=ssthresh+segments[(nv_ack-1)%buff_size].nbr_ACK_recv; // inflation de la fenêtre
        window_size = cwnd;
        congestion_avoidance = 1; // on passe directement en CONGESTION AVOIDANCE
    }else{	
        //printf("SLOW START => CONGESTION AVOIDANCE\n");
        cwnd = WINDOW;    
        window_size = cwnd;
        congestion_avoidance = 1;   
    }
    if(fast_retransmit && fast_recovery){
        deflate_ack = nv_ack+1; // l'ack qu'on attend pour deflate la fenêtre
    }
    // fseek(fp,(*seq-1)*(SEGMENT_SIZE-NUMSEQ_SIZE),SEEK_SET); // on se place au bon endroit dans le fichier
    //printf("Taille de fenêtre : %d\n",window_size);
}

void free_space(struct segment segments[], int index){
    // on va itérer dans le tableau pour trouver la case qui contient le numéro de séquence précédent et libérer la case
    // on va commencer par la case précédente 
    //printf("début fonction free space \n");
    int i=0;
    int found=0;
    int ind=0;
    int y=0;
    //printf("recherche case qui contient le segment n°%d-1\n",segments[index].seq);
    if(nv_ack!=1){ // on ne libére la case du premier message que quand on a reçu l'ACK du deuxième
        while((i+y)<buff_size && !found){
            ind = index; 
            if((index-i)>=0){
                //printf("index = %d - %d \n",index,i);
                ind = (index-i);
                i++;
            }
            else{
                // printf("index = (%d - (-%d)) +1\n",index,i);
                //printf("index = (%d - (%d) +1\n",buff_size,y);
                //ind = (index+(buff_size-1)+i)%buff_size; fonctionne mais on regarde pas à partir de la fin 
                ind = (buff_size-1)-y; // pour regarder en partant de la fin 
                y++;
            }
            //printf("Vérification de la case : %d\n",ind);
            if(segments[ind].seq<=nv_ack-1 && segments[ind].init!=0){ // si la case contient le segment précédent et que elle n'a pas été déjà libéré
                segments[ind].init = 0; // on la libère 
                segments[ind].nbr_ACK_recv = 0; // on la libère 
                //printf("libération de la case n°%d du tableau qui contient le segment %d\n",(ind),segments[ind].seq);
                found = 1;
            }
            
        }
        if(!found){
            //printf("Aucune case à libérer trouvé !\n");
            //exit(0);
        }
    }else{
        //printf("premier segment, on ne libère rien\n");
    }

}

void maj_ACK(struct segment segments[]){
    int i = (nv_ack-max_ack); // le nombre de nouveau segments à ACK
    int ack_seq;
    //printf("%d nouveaux segments à ACK, %d-%d \n",i,nv_ack,max_ack);
    while(i>=0){
        //printf("dans la boucle\n");
        if(i==0 && max_ack==nv_ack){ // si c'est un DACK
            // printf("DACK, aucun espace libéré \n");
            ack_seq = nv_ack-1;
        }
        else if(i!=0 && max_ack!=nv_ack){ 
            ack_seq = (max_ack+i)-1;
        }
        else{
            i--;
            continue;
        }
        
        segments[(ack_seq)%buff_size].nbr_ACK_recv += 1;
        //printf("Nombre ACK recu pour %d : %d\n",ack_seq+1,segments[ack_seq].nbr_ACK_recv);
        if(segments[(ack_seq)%buff_size].nbr_ACK_recv==1){ // si c'est le premier ACK reçu
            flightsize--;
            // segments[(ack_seq)%buff_size].init = 0; // on libère l'espace de la case précédente
            //printf("avant de libérer l'espace\n");
            free_space(segments,(ack_seq)%buff_size);
            if(flightsize<0){
                printf("error negative flightsize\n");
                exit(0);
            }
        } 
        gettimeofday(&segments[(ack_seq)%buff_size].last_ACK_recv_time, NULL);
        //printf("Case n°%d du tableau pour le segment n°%d\n",(ack_seq)%buff_size,ack_seq+1);
        //printf("Last Recv : %f secondes et  %f millisecondes  \n",(double)segments[(ack_seq)%buff_size].last_ACK_recv_time.tv_sec,(double)segments[(ack_seq)%buff_size].last_ACK_recv_time.tv_usec);
        maj_timer(segments,ack_seq+1);
        i--;
    }
}

void handle_ACK(struct segment segments[]){
    //printf("%d segments à acquitter\n",i);
    maj_ACK(segments);
    if(window_size>ssthresh && congestion_avoidance){ // CONGESTION AVOIDANCE => cwnd += 1/cwnd pour chaque ACK
        //printf("cwnd = %d \n",cwnd);
        cwnd += (nv_ack-max_ack)/(cwnd);    
    }else{ // SLOW START => cwnd +=1 pour chaque ACK 
        cwnd += nv_ack-max_ack;
    }              
    max_ack = nv_ack;
    // si pas dans l'ordre, donc inférieur au plus grand reçu => pas de pb 
    // si duplicate alors => retransmission 
    if(segments[(nv_ack-1)%buff_size].nbr_ACK_recv >= 3){ // si on a reçu 3 fois ou plus le même ACK
        lost_seg ++;
        handle_DACK(segments);
    }else if(segments[(nv_ack-1)%buff_size].nbr_ACK_recv > 1){ // si DUPLICATE ACK mais le 2e seulement
        //printf("1er DUPLICATE ACK \n");
        if(fast_retransmit && fast_recovery){
            seq = nv_ack+1;
            window_size =  (cwnd == 0) ? 1 : cwnd;
            // cwnd ne change pas et windows size est à 1 
        }
    }else{ // on a reçu un ACK qui n'est pas dupliqué on met à jour la fenêtre et on libère le segment
        window_size =  (cwnd == 0) ? 1 : cwnd;
    }
}

void handle_timeout(){
    seq = max_ack+1;
    ssthresh = flightsize/2;    
    if(ssthresh==0){
        ssthresh = 2; // le seuil ne peut pas être à 0 ou à 1 
    }
    cwnd = WINDOW;
    congestion_avoidance = 1;    
    window_size = cwnd;
    lost_ack ++;
}


void recv_ACK(int data_socket, struct sockaddr_in data_addr, int len, struct segment segments[], FILE *fp, int *retval){
    char buffer_ack[10]; // + 4 car on a "ACK" au début 
    // bzero(buffer_ack,sizeof(buffer_ack));
    recvfrom(data_socket, (char *)buffer_ack, 10, MSG_WAITALL, ( struct sockaddr *) &data_addr, &len);
    // printf("recv : %d\n",recv);
    char message[4];
    memcpy(message,buffer_ack,3);
    message[3] = '\0';
    char ack_char[7];
    memcpy(ack_char,buffer_ack+3,6);
    ack_char[6] = '\0';
    if(strcmp(message,"ACK")==0){
        nv_ack = atoi(ack_char);
        //printf("Client : ACK n°%d\n", nv_ack);
        //printf("Dernier ACK reçu : n°%d\n", max_ack);
        if(nv_ack==deflate_ack){ // si on a reçu l'ack suivant l'ack qu'on a reçu en dupliqué 
                //printf("DEFLATE ACK reçu\n");
                ssthresh = flightsize/2;
                cwnd=ssthresh; // deflation de la fenêtre
                cwnd = (ssthresh == 0) ? 1 : ssthresh;
        }
        if(nv_ack>=max_ack && nv_ack!= 0){
            handle_ACK(segments);
        }else{ // si on a bien reçu un ACK mais qui est dans le mauvais ordre 
            if(timer_set = 1){ // si notre fenêtre était à 0, alors on considère que le serveur n'a pas reçu d'ACK  
                *retval = 0;
            }
        }
    }
}


void init_segment(struct segment segments[], char str[], int retrans_seg){
    // int i = 0; 
    int index = (seq-1)%buff_size;
    // int found = 0;
    if(!retrans_seg){
        
        if(segments[index].init!=1 || (segments[index+1].nbr_ACK_recv != 0 && seq>buff_size)){ //si on a jamais transmis ce segment ou si le suivant a été ACK (utile quand on est dans le cas d'un buffer circulaire)
            //found = 1;
            //printf("Case n°%d du tableau pour le segment n°%d contient : %s\n",index,seq,str);
            segments[index].init = 1;
            segments[index].byte_length = res_read;
            memcpy(segments[index].bytes,str,res_read);
            segments[index].nbr_ACK_recv = 0;
            segments[index].seq = seq;
            flightsize++;
        }else{ // si la case n'est pas libre alors on attend un ACK
            //printf("Case n'est pas libre\n");
        }
        //gettimeofday(&(segments[index].trans_time),NULL);
    }
    else{
        //printf("retransmission\n");
        retransmission++;
    }
    gettimeofday(&(segments[index].trans_time),NULL);
}

void wait_for_ACK(int data_socket, struct sockaddr_in data_addr, int len, struct segment segments[], FILE *fp){
    int retval = 0;
    fd_set read_fds;
    FD_ZERO(&read_fds); // création du set qui va contenir la socket
    FD_SET(data_socket, &read_fds); // met la socket de données dans le set
    struct timeval tv; // le timer pour un ACK
    if(window_size==0 || seq==(int)nbre_seg+1){ // si la fenêtre est à 0 alors on attend un ACK avec un timer de valeur RTT 
        timer_set = 1;
        tv.tv_sec = 0; // le nombre de secondes qu'on va attendre, va être en fonction du RTT (pour l'instant 5 sec)
        tv.tv_usec = 1.5*timer; // ... millisecondes .., on met 2*le RTT car il y a de la latence
        if(timer<0){
            printf("error, timer négatif \n");
            exit(0);
        }
        //printf("%d millisecondes d'attente \n",(int)tv.tv_usec);
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
            //printf("ACK reçu \n");
            recv_ACK(data_socket,data_addr,len,segments,fp,&retval);
        }
    }
    if(!retval){
        if(timer_set==1){ // si on a mis un timer (fenêtre à 0) et qu'on a pas reçu d'ACK ou qu'on a reçu un ACK qui était pour des segments précédents 
            //printf("TIMEOUT : Aucun ACK reçu, retransmission à partir de %d\n",max_ack+1); // le timer a expiré, on va retransmettre à partir de : segment suivant le dernier segment acquitté + 1 
            handle_timeout();
        }else{
            //printf("Aucun ACK reçu, message suivant\n");
        }
    }
    //printf("Nbres de messages envoyé pas encore acquitté : %d\n",flightsize);
    //printf("Taille de fenêtre : %d\n",window_size);
    timer_set = 0;  
}

void send_segment(int data_socket, struct sockaddr_in data_addr, struct segment segments[], FILE *fp, char str[], char seq_char[], int len_str){
    int bytes_sent;
    int retrans_seg = 0;
    // printf("str avant de lire : %s\n",str);
    // printf("Taille de str : %d\n",len_str);
    struct timeval current;
    struct timeval res;
    if(rto_enable){
        gettimeofday(&current, NULL);
        timersub(&current, &segments[(seq-1)%buff_size].trans_time,&res);
        //printf("Seconds = %d - %d, Millisecondes = %d - %d => Secondes = %d, ms = %d\n",(int)current.tv_sec,(int)segments[(seq-1)%buff_size].trans_time.tv_sec,(int)current.tv_usec,(int)segments[(seq-1)%buff_size].trans_time.tv_usec,(int)res.tv_sec,(int)res.tv_usec);
        //printf("Wait %d milliseconds before sending segment n°%d\n",(int)(res.tv_sec*1000+res.tv_usec),seq);

    }
    if( (rto_enable && segments[(seq-1)%buff_size].init==1 && (res.tv_sec*1000000+res.tv_usec)>rto) || (!rto_enable) || (segments[(seq-1)%buff_size].init!=1) ){
        if(segments[(seq-1)%buff_size].init==1){ // si on a déjà transmis on prend les octets stockés en mémoire 
        memcpy(str,segments[(seq-1)%buff_size].bytes,segments[(seq-1)%buff_size].byte_length);
        res_read = segments[(seq-1)%buff_size].byte_length;
        retrans_seg = 1;
            //printf("Retransmission\n");
        }else{ // sinon on lit dans le fichier
            res_read = fread(str,1,len_str,fp); 
            nbre_octets+= res_read;
        }
        // printf("str après avoir lu : %s\n",str);        
        // si on est pas au dernier segment alors on envoie
        //printf("%d octets lus\n",res_read);
        sprintf(seq_char,"%06d" ,seq);
        memcpy(seq_char+NUMSEQ_SIZE,str,res_read);
        //printf("str après avoir cpy : %s\n",str);
        // printf("Message seq_char envoyé : %s\n",seq_char);
        // printf("Taille de str : %d\n",(int)strlen(str));
        //printf("Taille du segment : %d\n",(int)strlen(seq_char));
        if(res_read==0){
            printf("error, Aucun octet lus pour le segment %d\n",seq);
            //printf("Case n°%d du tableau (init = %d) pour le segment n°%d contient : %s et a reçu %d ACK\n",(seq-1)%buff_size,segments[(seq-1)%buff_size].init,segments[(seq-1)%buff_size].seq,segments[(seq-1)%buff_size].bytes,segments[(seq-1)%buff_size].nbr_ACK_recv);
            exit(0);
        }
        bytes_sent = sendto(data_socket, (const char *)seq_char, res_read+NUMSEQ_SIZE, MSG_CONFIRM, (const struct sockaddr *) &data_addr, sizeof(data_addr));
        res_sent += bytes_sent; 
        //printf("Message n°%d sent : %d bytes\n",seq,bytes_sent);
        init_segment(segments,str,retrans_seg);
        seq++; 
        window_size --;
    }else{
        //printf("Wait = %d - %d \n",(int)rto,(int)(res.tv_sec*1000000+res.tv_usec));
        //printf("Wait %d milliseconds before sending segment n°%d\n",(int)rto-(int)(res.tv_sec*1000000+res.tv_usec),seq);
    }
}

void segments_print(struct segment segments[]){
    int i = 0; 
    while(i<buff_size){
        printf("Case n°%d :\n",i);
        printf("init : %d\n",segments[i].init);
        printf("seq : %d\n",segments[i].seq);
        printf("ack reçu : %d\n",segments[i].nbr_ACK_recv);
        printf("message : %s\n",segments[i].bytes);
        i++;
    }
}

int exchange_file(int data_socket, struct sockaddr_in data_addr){
    char buffer[128];
    bzero(buffer,sizeof(buffer));
    int n, len;
    len = sizeof(data_addr);
    // entrée dans la connexion 
    n = recvfrom(data_socket, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &data_addr, &len);
    buffer[n] = '\0'; 
    //printf("Client sent filename : %s\n", buffer);
    FILE *fp;
    char str[SEGMENT_SIZE-NUMSEQ_SIZE]; // va contenir ce qu'on lit dans le fichier
    bzero(str,sizeof(str));
    /* opening file for reading */
    fp = fopen(buffer , "rb"); // ouverture du fichier
    if(fp == NULL) {
        perror("Error opening file");
        return(-1);
    }
    char seq_char[SEGMENT_SIZE];
    struct stat st;
    stat(buffer, &st);
    size_t file_size = st.st_size;
    //printf("Taille du fichier : %d\n",(int)file_size);
    nbre_seg = ceil((double)file_size/(double)(SEGMENT_SIZE-NUMSEQ_SIZE)); // on arrondi à l'entier supérieur pour connaître le nombre de segments nécessaires
    //printf("%d segments nécessaires\n",(int)nbre_seg);
    if(nbre_seg>BUFFER_SIZE){
        printf("fichier trop grand \n");
        //exit(0);
        buff_size = BUFFER_SIZE;
    }else{
        buff_size = nbre_seg;
    }
    struct segment* segments = malloc(buff_size * sizeof(struct segment));
    window_size = cwnd;
    while(max_ack!=(int)nbre_seg){ // tant qu'on a pas reçu le dernier ACK
        if(seq<(int)nbre_seg+1){
            if ((segments[(seq-1)%buff_size].init!=1 || segments[(seq-1)%buff_size].nbr_ACK_recv == 0) && seq>max_ack){ // si on ne l'a jamais envoyé ou si on n'a pas reçu d'ACK et qu'on est pas au segment suivant le dernier
                send_segment(data_socket,data_addr,segments,fp,str,seq_char,SEGMENT_SIZE-NUMSEQ_SIZE);
                //printf("%d segments nécessaires\n",(int)nbre_seg);
            }
            else if(segments[(seq-1)%buff_size].seq != seq){ // si la case contient déjà un segment et n'a pas été libéré 
                //printf("impossible d'envoyer message n°%d\n",seq);
                //printf("Case n°%d contient segment n°%d, pas encore libéré, attente d'ACK\n",(seq-1)%buff_size,segments[(seq-1)%buff_size].seq);
                //printf("Retransmission à partir de : %d\n",max_ack+1);
                seq = max_ack+1;
                // segments_print(segments);
                //exit(0);
            }
            else{ // sinon on passe au suivant
                // printf("Case n°%d du tableau pour le segment %d\n",(seq-1)%buff_size,seq);
                //printf("Segment %d ALREADY ACKD \n",seq);
                //printf("Case n°%d du tableau pour le segment n°%d contient : %s et a reçu %d ACK\n",(seq-1)%buff_size,segments[(seq-1)%buff_size].seq,segments[(seq-1)%buff_size].bytes,segments[(seq-1)%buff_size].nbr_ACK_recv);
                //exit(0);
                seq++;
            }
        }
        wait_for_ACK(data_socket,data_addr,len,segments,fp);
        // if(window_size>1){
        //     printf("Fenêtre : [%d,%d]\n",seq,seq+window_size);
        // }
        // else if(window_size==1){
        //     printf("Fenêtre : [%d]\n",seq);
        // }
        // else{
        //     printf("Fenêtre : []\n"); // DEBUG : la fenêtre ne peut jamais être à 0, on a toujours des messages à envoyer
        //     exit(0);
        // }
    }
    free(segments);
    char eof[4] = "FIN";
    sendto(data_socket, (const char *)eof, strlen(eof), MSG_CONFIRM, (const struct sockaddr *) &data_addr, sizeof(data_addr));
    fclose(fp);
    close(data_socket);
    // printf("fichier de %d octets\n",(int)file_size);
    // printf("%d octets lus et %d octets envoyés\n",(int)nbre_octets,res_sent);
    // printf("%d segments perdus sur %d initiaux\n",(int)lost_seg,(int)nbre_seg);
    // printf("%d ACK perdus\n",(int)lost_ack);
    //segments_print(segments);
    return 1;
}


int twh_serv(int sockfd, struct sockaddr_in cliaddr){
    // printf("nv twh\n");
    int len, n;
    char buffer[4];
    bzero(buffer,sizeof(buffer));
	len = sizeof(cliaddr);//taille de la structure qui mène vers le client
    n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len);
	buffer[n] = '\0';
	//printf("Client : %s\n", buffer);
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
        // message[12] = '\0';
        sendto(sockfd, (const char *)message, strlen(message), MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);
        n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len);
        buffer[n] = '\0'; 
	    //printf("Client : %s\n", buffer);
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
    //printf("Début main \n");
    int public_port;
    int alpha;
    if(argc >= 2){
		public_port = atoi(argv[1]);
	}else{
		public_port = PORT;
	}

    if(argc >= 3){
        //printf("alpha = %f\n",atof(argv[2]));
		alpha = atof(argv[2]);
	}else{
		alpha = 0.9;
	}
    

	int sockfd;
	struct sockaddr_in servaddr, cliaddr;
	memset(&cliaddr, 0, sizeof(cliaddr));
    createsocket(&sockfd,&servaddr,public_port);

    while(1){
        reset_var(alpha);
        //printf("Variable init\n");
        int res_twh = twh_serv(sockfd,cliaddr); // twh correspond au connect en TCP
        //printf("res_twh\n");
        if(res_twh == 1){
            //printf("twh successful \n");
            //system("cmp bigfile.txt copy_bigfile.txt");
            //exit(0);
            break;
        }
        // le fork est fait quand on fait la socket de données, le fils gère les données et le père acceptre les clients    
    }
    //printf("Fin main \n");
	return 0;
}
