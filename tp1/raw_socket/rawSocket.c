#include "rawSocket.h"

// Inicializa o raw socket
int rawsocket_init(rawsocket_t* rs, const char* interface) {
    if (!rs || !interface) return -1;
    
    memset(rs, 0, sizeof(rawsocket_t));
    strncpy(rs->interface, interface, IF_NAMESIZE - 1);
    
    // Criar raw socket
    rs->sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (rs->sockfd < 0) {
        perror("Erro ao criar raw socket");
        return -1;
    }
    
    // Obter informações da interface
    if (get_interface_info(interface, rs->src_mac, &rs->src_ip) < 0) {
        close(rs->sockfd);
        return -1;
    }
    
    // Configurar endereço do socket
    memset(&rs->socket_address, 0, sizeof(rs->socket_address));
    rs->socket_address.sll_family = AF_PACKET;
    rs->socket_address.sll_protocol = htons(ETH_P_IP);
    rs->socket_address.sll_ifindex = if_nametoindex(interface);
    rs->socket_address.sll_hatype = ARPHRD_ETHER;
    rs->socket_address.sll_pkttype = PACKET_OTHERHOST;
    rs->socket_address.sll_halen = ETH_ALEN;
    
    printf("Raw socket inicializado na interface %s\n", interface);
    printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", 
           rs->src_mac[0], rs->src_mac[1], rs->src_mac[2],
           rs->src_mac[3], rs->src_mac[4], rs->src_mac[5]);
    printf("IP: %s\n", inet_ntoa(*(struct in_addr*)&rs->src_ip));
    
    return 0;
}

// Define o destino
int rawsocket_set_destination(rawsocket_t* rs, const char* dest_ip, unsigned short dest_port) {
    if (!rs || !dest_ip) return -1;
    
    rs->dest_ip = inet_addr(dest_ip);
    rs->dest_port = htons(dest_port);
    
    // Tentar resolver MAC de destino
    if (resolve_mac_address(dest_ip, rs->dest_mac, rs->interface) < 0) {
        fprintf(stderr, "Aviso: Não foi possível resolver MAC do destino, usando broadcast\n");
        memset(rs->dest_mac, 0xFF, 6); // Broadcast MAC
    }
    
    memcpy(rs->socket_address.sll_addr, rs->dest_mac, 6);
    
    return 0;
}

// Define a porta de origem
int rawsocket_set_source(rawsocket_t* rs, unsigned short src_port) {
    if (!rs) return -1;
    
    rs->src_port = htons(src_port);
    return 0;
}

// Envia dados usando raw socket
int rawsocket_send(rawsocket_t* rs, const void* data, size_t data_len) {
    if (!rs || !data || data_len == 0) return -1;
    // Buffer para o pacote completo
    unsigned char packet[MAX_PACKET_SIZE];
    memset(packet, 0, sizeof(packet));
    
    // Calcular tamanhos
    size_t eth_header_size = sizeof(struct ethernet_header);
    size_t ip_header_size = sizeof(struct ip_header);
    size_t udp_header_size = sizeof(struct udp_header);
    size_t total_size = eth_header_size + ip_header_size + udp_header_size + data_len;
    
    if (total_size > MAX_PACKET_SIZE) {
        fprintf(stderr, "Pacote muito grande: %zu bytes\n", total_size);
        return -1;
    }
    
    // Cabeçalho Ethernet
    struct ethernet_header* eth_hdr = (struct ethernet_header*)packet;
    memcpy(eth_hdr->dest_mac, rs->dest_mac, 6);
    memcpy(eth_hdr->src_mac, rs->src_mac, 6);
    eth_hdr->eth_type = htons(0x0800); // IP
    
    // Cabeçalho IP
    struct ip_header* ip_hdr = (struct ip_header*)(packet + eth_header_size);
    ip_hdr->version_ihl = 0x45; // Versão 4, IHL 5 (20 bytes)
    ip_hdr->tos = 0;
    ip_hdr->total_length = htons(ip_header_size + udp_header_size + data_len);
    ip_hdr->id = htons(getpid());
    ip_hdr->frag_off = 0;
    ip_hdr->ttl = 64;
    ip_hdr->protocol = 17; // UDP
    ip_hdr->checksum = 0;
    ip_hdr->src_addr = rs->src_ip;
    ip_hdr->dest_addr = rs->dest_ip;
    
    // Calcular checksum IP
    ip_hdr->checksum = calculate_checksum((unsigned short*)ip_hdr, ip_header_size);
    
    // Cabeçalho UDP
    struct udp_header* udp_hdr = (struct udp_header*)(packet + eth_header_size + ip_header_size);
    udp_hdr->src_port = rs->src_port;
    udp_hdr->dest_port = rs->dest_port;
    udp_hdr->length = htons(udp_header_size + data_len);
    udp_hdr->checksum = 0;
    
    // Copiar dados
    memcpy(packet + eth_header_size + ip_header_size + udp_header_size, data, data_len);
    
    // Calcular checksum UDP
    udp_hdr->checksum = calculate_udp_checksum(ip_hdr, udp_hdr, 
                                              (unsigned char*)data, data_len);
    
    // Enviar pacote
    ssize_t sent = sendto(rs->sockfd, packet, total_size, 0,
                         (struct sockaddr*)&rs->socket_address,
                         sizeof(rs->socket_address));
    
    if (sent < 0) {
        perror("Erro ao enviar pacote");
        return -1;
    }
    
    return sent;
}

// Recebe dados usando raw socket
int rawsocket_receive(rawsocket_t* rs, void* buffer, size_t buffer_size,
                     unsigned int* src_ip, unsigned short* src_port) {
    if (!rs || !buffer) return -1;
    
    unsigned char packet[MAX_PACKET_SIZE];
    struct sockaddr_ll addr;
    socklen_t addr_len = sizeof(addr);
    
    ssize_t received = recvfrom(rs->sockfd, packet, sizeof(packet), 0,
                               (struct sockaddr*)&addr, &addr_len);
    
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -2; // Timeout
        }
        perror("Erro ao receber pacote");
        return -1;
    }
    
    // Verificar se é um pacote IP
    struct ethernet_header* eth_hdr = (struct ethernet_header*)packet;
    if (ntohs(eth_hdr->eth_type) != 0x0800) {
        return 0; // Não é IP, ignorar
    }
    
    // Verificar cabeçalho IP
    struct ip_header* ip_hdr = (struct ip_header*)(packet + sizeof(struct ethernet_header));
    if (ip_hdr->protocol != 17) {
        return 0; // Não é UDP, ignorar
    }
    
    // Verificar se é para nós
    if (ip_hdr->dest_addr != rs->src_ip) {
        return 0; // Não é para nós, ignorar
    }
    
    // Cabeçalho UDP
    struct udp_header* udp_hdr = (struct udp_header*)(packet + sizeof(struct ethernet_header) + sizeof(struct ip_header));
    
    // Verificar porta
    if (udp_hdr->dest_port != rs->src_port) {
        return 0; // Não é para nossa porta, ignorar
    }
    
    // Extrair dados
    size_t header_size = sizeof(struct ethernet_header) + sizeof(struct ip_header) + sizeof(struct udp_header);
    size_t data_size = ntohs(udp_hdr->length) - sizeof(struct udp_header);
    
    if (data_size > buffer_size) {
        fprintf(stderr, "Buffer muito pequeno para os dados recebidos\n");
        return -1;
    }
    
    memcpy(buffer, packet + header_size, data_size);
    
    // Retornar informações do remetente se solicitado
    if (src_ip) *src_ip = ip_hdr->src_addr;
    if (src_port) *src_port = ntohs(udp_hdr->src_port);
    
    return data_size;
}

// Fecha o raw socket
void rawsocket_close(rawsocket_t* rs) {
    if (rs && rs->sockfd >= 0) {
        close(rs->sockfd);
        rs->sockfd = -1;
    }
}

// Calcula checksum
unsigned short calculate_checksum(unsigned short* ptr, int nbytes) {
    long sum = 0;
    unsigned short oddbyte;
    
    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }
    
    if (nbytes == 1) {
        oddbyte = 0;
        *((unsigned char*)&oddbyte) = *(unsigned char*)ptr;
        sum += oddbyte;
    }
    
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (unsigned short)~sum;
}

// Calcula checksum UDP
unsigned short calculate_udp_checksum(struct ip_header* ip_hdr, struct udp_header* udp_hdr, 
                                     unsigned char* data, int data_len) {
    // Para simplificar, retorna 0 (checksum UDP é opcional em IPv4)
    return 0;
}

// Obtém informações da interface
int get_interface_info(const char* interface, unsigned char* mac, unsigned int* ip) {
    if (!interface || !mac || !ip) return -1;
    
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Erro ao criar socket para obter info da interface");
        return -1;
    }
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface, IF_NAMESIZE - 1);
    
    // Obter MAC
    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0) {
        perror("Erro ao obter MAC da interface");
        close(sockfd);
        return -1;
    }
    memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
    
    // Obter IP
    if (ioctl(sockfd, SIOCGIFADDR, &ifr) < 0) {
        perror("Erro ao obter IP da interface");
        close(sockfd);
        return -1;
    }
    *ip = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr.s_addr;
    
    close(sockfd);
    return 0;
}

// Resolve endereço MAC (implementação simplificada)
int resolve_mac_address(const char* dest_ip, unsigned char* dest_mac, const char* interface) {
    // Implementação simplificada - usa broadcast MAC
    // Em uma implementação completa, seria necessário fazer ARP
    memset(dest_mac, 0xFF, 6); // Broadcast MAC
    return 0;
}
