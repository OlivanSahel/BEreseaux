#include <mictcp.h>
#include <api/mictcp_core.h>
#include <stdbool.h>
#define MAX_SOCK 100
#define LOSS_RATE 50   // pourcentage de perte

// mic_tcp_sock tab_sock[MAX_SOCK];
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
    return 0;
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send(int mic_sock, char *mesg, int mesg_size) {
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");

    mic_tcp_pdu pdu = {0};
    pdu.header.source_port = 9000;
    pdu.header.dest_port = sock.remote_addr.port;
    pdu.header.seq_num = PA;
    pdu.header.ack_num = 0;
    pdu.payload.data = mesg;
    pdu.payload.size = mesg_size;

    int weReceiveAck = 1;
    mic_tcp_pdu pduAck;
    int taillePDU = 0;
    while(weReceiveAck == 1){
        taillePDU = IP_send(pdu, sock.remote_addr.ip_addr); 
        int res = IP_recv(&pduAck, &sock.local_addr.ip_addr, &sock.remote_addr.ip_addr, 200); // Timeout
        if (res < 0){
            weReceiveAck = 1;
        }
        else{
            weReceiveAck = 0;
        }
    }
    PA = (PA + 1) % 2;
    return taillePDU; // v2
    // return IP_send(pdu, sock.remote_addr.ip_addr); // v1
    
}

/*
 * Permet à l’application réceptrice de réclamer la récupération d’une donnée
 * stockée dans les buffers de réception du socket
 * Retourne le nombre d’octets lu ou bien -1 en cas d’erreur
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
    return -1;
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
    //app_buffer_put(pdu.payload); // v1

    if (pdu.header.seq_num == PA) { // v2
        PA = (PA + 1) % 2;
        app_buffer_put(pdu.payload);
    }

    // Rien à faire pour la v3 ici

    mic_tcp_pdu Ackpdu = {0};
    Ackpdu.header.source_port = 9000;
    Ackpdu.header.dest_port = sock.remote_addr.port;
    Ackpdu.header.seq_num = PA;
    Ackpdu.header.ack_num = PA;
    Ackpdu.header.ack = 1; /* flag ACK (valeur 1 si activé et 0 si non) */
    IP_send(Ackpdu, remote_addr);
}

