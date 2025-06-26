#ifndef RAWSOCKET_H
#define RAWSOCKET_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <linux/if_arp.h>    // Para ARPHRD_ETHER
#include <net/ethernet.h>
#include <errno.h>

#define MAX_PACKET_SIZE 65536
#define INTERFACE_NAME "enp0s31f6"  // Interface padrão - pode ser alterada

// Estrutura para o cabeçalho Ethernet
struct ethernet_header {
    unsigned char dest_mac[6];   // MAC de destino
    unsigned char src_mac[6];    // MAC de origem
    unsigned short eth_type;     // Tipo (0x0800 para IP)
};

// Estrutura para o cabeçalho IP
struct ip_header {
    unsigned char version_ihl;   // Versão (4 bits) + IHL (4 bits)
    unsigned char tos;           // Tipo de Serviço
    unsigned short total_length; // Comprimento total
    unsigned short id;           // Identificação
    unsigned short frag_off;     // Flags de fragmentação
    unsigned char ttl;           // Time to Live
    unsigned char protocol;      // Protocolo (17 para UDP)
    unsigned short checksum;     // Checksum do IP
    unsigned int src_addr;       // Endereço IP de origem
    unsigned int dest_addr;      // Endereço IP de destino
};

// Estrutura para o cabeçalho UDP
struct udp_header {
    unsigned short src_port;     // Porta de origem
    unsigned short dest_port;    // Porta de destino
    unsigned short length;       // Comprimento UDP
    unsigned short checksum;     // Checksum UDP
};

// Estrutura para o contexto do raw socket
typedef struct {
    int sockfd;                          // File descriptor do socket
    struct sockaddr_ll socket_address;   // Endereço do socket
    char interface[IF_NAMESIZE];            // Nome da interface
    unsigned char src_mac[6];            // MAC de origem
    unsigned char dest_mac[6];           // MAC de destino
    unsigned int src_ip;                 // IP de origem
    unsigned int dest_ip;                // IP de destino
    unsigned short src_port;             // Porta de origem
    unsigned short dest_port;            // Porta de destino
} rawsocket_t;

// Funções principais
int rawsocket_init(rawsocket_t* rs, const char* interface);
int rawsocket_set_destination(rawsocket_t* rs, const char* dest_ip, unsigned short dest_port);
int rawsocket_set_source(rawsocket_t* rs, unsigned short src_port);
int rawsocket_send(rawsocket_t* rs, const void* data, size_t data_len);
int rawsocket_receive(rawsocket_t* rs, void* buffer, size_t buffer_size, 
                     unsigned int* src_ip, unsigned short* src_port);
void rawsocket_close(rawsocket_t* rs);

// Funções auxiliares
unsigned short calculate_checksum(unsigned short* ptr, int nbytes);
unsigned short calculate_udp_checksum();
int get_interface_info(const char* interface, unsigned char* mac, unsigned int* ip);
int resolve_mac_address(unsigned char* dest_mac);

#endif // RAWSOCKET_H
