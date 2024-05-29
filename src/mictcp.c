#include <mictcp.h>
#include <api/mictcp_core.h>
#include <stdbool.h>

#define MAX_SOCK 100
#define LOSS_RATE 20   // pourcentage de perte par défaut

unsigned int* lostArray = NULL;
int LOST_RATIO = 20; // Pourcentage de perte accepté (entier)
int LOST_ARRAY_SIZE = 0;

mic_tcp_sock sock = {0};
int PA = 0;

/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm)
{
    int result = -1;
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    result = initialize_components(sm); /* Appel obligatoire */
    set_loss_rate(LOSS_RATE);

    return result;
}

/*
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    sock.local_addr = addr;
    return 0;
}

/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");

    // Receive SYN from client
    mic_tcp_pdu syn_pdu = {0};
    IP_recv(&syn_pdu, &sock.local_addr.ip_addr, &sock.remote_addr.ip_addr, 2000);
    if (syn_pdu.header.syn != 1) {
        return -1;
    }

    // Send SYN-ACK with proposed loss ratio
    LOST_RATIO = syn_pdu.payload.data[0];
    LOST_ARRAY_SIZE = (100 / LOST_RATIO) + 1;
    lostArray = malloc(sizeof(unsigned int) * LOST_ARRAY_SIZE);
    memset(lostArray, 0, sizeof(unsigned int) * LOST_ARRAY_SIZE);

    mic_tcp_pdu syn_ack_pdu = {0};
    syn_ack_pdu.header.syn = 1;
    syn_ack_pdu.header.ack = 1;
    syn_ack_pdu.payload.data = (char*)&LOST_RATIO;
    syn_ack_pdu.payload.size = sizeof(int);
    IP_send(syn_ack_pdu, sock.remote_addr.ip_addr);

    // Receive ACK from client
    mic_tcp_pdu ack_pdu = {0};
    IP_recv(&ack_pdu, &sock.local_addr.ip_addr, &sock.remote_addr.ip_addr, 2000);
    if (ack_pdu.header.ack != 1) {
        return -1;
    }

    *addr = sock.remote_addr;
    return 0;
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    sock.remote_addr = addr;

    // Envoie SYN avec le ratio de perte dans le payload
    mic_tcp_pdu syn_pdu = {0};
    syn_pdu.header.syn = 1;
    syn_pdu.payload.data = (char*)&LOST_RATIO;
    syn_pdu.payload.size = sizeof(int);
    IP_send(syn_pdu, sock.remote_addr.ip_addr);

    // attend Syn ACK
    mic_tcp_pdu syn_ack_pdu = {0};
    IP_recv(&syn_ack_pdu, &sock.local_addr.ip_addr, &sock.remote_addr.ip_addr, 2000);
    if (syn_ack_pdu.header.syn != 1 || syn_ack_pdu.header.ack != 1) {
        return -1;
    }

    LOST_RATIO = *(int*)syn_ack_pdu.payload.data;
    LOST_ARRAY_SIZE = (100 / LOST_RATIO) + 1;
    lostArray = malloc(sizeof(unsigned int) * LOST_ARRAY_SIZE);
    memset(lostArray, 0, sizeof(unsigned int) * LOST_ARRAY_SIZE);

    // ACK
    mic_tcp_pdu ack_pdu = {0};
    ack_pdu.header.ack = 1;
    IP_send(ack_pdu, sock.remote_addr.ip_addr);

    return 0;
}

void rightShiftLostArray(int valueFirstCase)
{
    for (int i = LOST_ARRAY_SIZE - 1; i > 0; i--) { // Décalage à droite dans le tableau
        lostArray[i] = lostArray[i - 1];
    }
    lostArray[0] = valueFirstCase;
}

int weAcceptLoss()
{
    for (int i = 0; i < LOST_ARRAY_SIZE; i++) { // On regarde si il y a un 1 dans le tableau => on ne pourrait pas accepter la perte
        if (lostArray[i] == 1) {
            return false; // We do not accept loss
        }
    }
    return true; // We accept loss
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send(int mic_sock, char *mesg, int mesg_size) {
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");

    mic_tcp_pdu pdu = {0};
    pdu.header.source_port = sock.local_addr.port;
    pdu.header.dest_port = sock.remote_addr.port;
    pdu.header.seq_num = PA;
    pdu.header.ack_num = 0;
    pdu.payload.data = mesg;
    pdu.payload.size = mesg_size;

    int ok = false;
    mic_tcp_pdu pduAck = {0};
    int taillePDU = 0;
    int res = 0;
    while(ok == false){
        res = 0;
        taillePDU = IP_send(pdu, sock.remote_addr.ip_addr); 
        res = IP_recv(&pduAck, &sock.local_addr.ip_addr, &sock.remote_addr.ip_addr, 200); 
        printf("res = %d \n", res);
        if (res == -1){ //timeout
            if(weAcceptLoss()){
                printf("on accept la perte \n");
                ok = true;
            }
            else{
                printf("on n'accept PAS la perte \n");
                ok = false;
            }
            rightShiftLostArray(1);
        }
        else{ // on a reçu le ack, on sort de la boucle.
            rightShiftLostArray(0);
            ok = true;
        }
    }
    PA = (PA + 1) % 2;
    return taillePDU; // v2
}

/*
 * Permet à l’application réceptrice de réclamer la récupération d’une donnée
 * stockée dans les buffers de réception du socket
 * Retourne le nombre d’octets lu ou bien -1 en cas d'erreur
 * NB : cette fonction fait appel à la fonction app_buffer_get()
 */
int mic_tcp_recv(int socket, char* mesg, int max_mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");

    mic_tcp_payload payload = {0};
    payload.data = mesg;
    payload.size = max_mesg_size;

    int read_size = app_buffer_get(payload);

    return read_size;
}

/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close(int socket)
{
    printf("[MIC-TCP] Appel de la fonction :  "); printf(__FUNCTION__); printf("\n");
    if (lostArray != NULL) {
        free(lostArray);
    }
    return 0;
}

/*
 * Traitement d’un PDU MIC-TCP reçu (mise à jour des numéros de séquence
 * et d'acquittement, etc.) puis insère les données utiles du PDU dans
 * le buffer de réception du socket. Cette fonction utilise la fonction
 * app_buffer_put().
 */
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_ip_addr local_addr, mic_tcp_ip_addr remote_addr)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");

    if (pdu.header.seq_num == PA) {
        PA = (PA + 1) % 2;
        app_buffer_put(pdu.payload);
    }

    mic_tcp_pdu Ackpdu = {0};
    Ackpdu.header.source_port = sock.local_addr.port;
    Ackpdu.header.dest_port = sock.remote_addr.port;
    Ackpdu.header.seq_num = PA;
    Ackpdu.header.ack_num = PA;
    Ackpdu.header.ack = 1; /* flag ACK (valeur 1 si activé et 0 si non) */
    IP_send(Ackpdu, remote_addr);
}

