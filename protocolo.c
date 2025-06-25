#include "protocolo.h"

uint8_t getSeq(pack_t pack){
    return pack.seq_bit0 | (pack.seq_bits1_4 << 1);
}

void limpar_tela() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}


int reenvio(estado_protocolo_t* estado, pack_t pack){
if((pack.tipo == TIPO_ACK)||(pack.tipo == TIPO_NACK)||(pack.tipo == TIPO_OK_ACK))
        return 0;
    return enviar_pack(estado, &pack);
}

int seqCheck(uint8_t seqAtual, uint8_t seqPack){
    if(seqAtual == seqPack){
        return 0;
    } else if (seqPack == ((seqAtual + 1) % 32)){
        return 1;
    } else{
        return -1;
    }

}



// --- Cálculo de checksum ---
uint8_t calcular_checksum(const pack_t* p) {
    uint8_t checksum = 0;
    checksum ^= p->tamanho;
    checksum ^= (p->seq_bit0 | (p->seq_bits1_4 << 1));
    checksum ^= p->tipo;
    for (int i = 0; i < p->tamanho; i++) {
        checksum ^= p->dados[i];
    }
    return checksum;
}

// --- Funções de inicialização do protocolo ---
int inicializar_protocolo_servidor(estado_protocolo_t* estado, const char* interface) {
    if (!estado || !interface) return -1;
    
    memset(estado, 0, sizeof(estado_protocolo_t));
    
    // Inicializar raw socket
    if (rawsocket_init(&estado->rawsock, interface) < 0) {
        fprintf(stderr, "Erro ao inicializar raw socket do servidor\n");
        return -1;
    }
    
    // Configurar porta de origem (servidor usa porta fixa)
    if (rawsocket_set_source(&estado->rawsock, PORTA) < 0) {
        fprintf(stderr, "Erro ao configurar porta do servidor\n");
        rawsocket_close(&estado->rawsock);
        return -1;
    }
    
    estado->seq_atual = 0;
    estado->seq_esperada = 0;
    estado->src_port = PORTA;
    
    printf("Servidor inicializado na porta %d\n", PORTA);
    return 0;
}

int inicializar_protocolo_cliente(estado_protocolo_t* estado, const char* dest_ip, unsigned short orig_port,
                                 unsigned short dest_port, const char* interface) {
    if (!estado || !dest_ip || !interface) return -1;
    
    memset(estado, 0, sizeof(estado_protocolo_t));
    
    // Inicializar raw socket
    if (rawsocket_init(&estado->rawsock, interface) < 0) {
        fprintf(stderr, "Erro ao inicializar raw socket do cliente\n");
        return -1;
    }
    
    // Configurar destino
    strncpy(estado->dest_ip, dest_ip, sizeof(estado->dest_ip) - 1);
    estado->dest_port = dest_port;
    
    if (rawsocket_set_destination(&estado->rawsock, dest_ip, dest_port) < 0) {
        fprintf(stderr, "Erro ao configurar destino\n");
        rawsocket_close(&estado->rawsock);
        return -1;
    }
    
    // Porta de origem aleatória para cliente
    estado->src_port = orig_port;
    if (rawsocket_set_source(&estado->rawsock, estado->src_port) < 0) {
        fprintf(stderr, "Erro ao configurar porta do cliente\n");
        rawsocket_close(&estado->rawsock);
        return -1;
    }
    
    estado->seq_atual = 0;
    estado->seq_esperada = 0;
    
    printf("Cliente inicializado, conectando para %s:%d (porta local: %d)\n", 
           dest_ip, dest_port, estado->src_port);
    return 0;
}

void finalizar_protocolo(estado_protocolo_t* estado) {
    if (estado) {
        rawsocket_close(&estado->rawsock);
        memset(estado, 0, sizeof(estado_protocolo_t));
    }
}

int criar_pack(pack_t* pack, unsigned char seq, tipo_msg_t tipo,
               uint8_t* dados, unsigned short tamanho) {
    if (!pack || tamanho > 127 || seq > 31 || tipo > 15) {
        printf("Erro: valores fora dos limites do cabeçalho\n");
        return -1;
    }

    pack->marcador = 0x7E;
    pack->tamanho = tamanho & 0x7F;      // Garante 7 bits
    pack->seq_bit0 = seq & 0x01;         // Pega o bit 0 da sequência
    pack->seq_bits1_4 = (seq >> 1) & 0x0F; // Pega bits 1-4
    pack->tipo = tipo & 0x0F;            // Garante 4 bits

    // Limpar dados antes de copiar
    memset(pack->dados, 0, sizeof(pack->dados));
    
    if (dados && tamanho > 0) {
        memcpy(pack->dados, dados, tamanho);
    }

    // Aplicar checksum
    pack->checksum = calcular_checksum(pack);

    return 0;
}

int enviar_pack(estado_protocolo_t* estado, const pack_t* pack) {
    if (!estado || !pack) return -1;

    int tamanho_total = 4 + pack->tamanho;

    
    for (int tentativa = 0; tentativa < MAX_TENTATIVAS; tentativa++) {
        int enviados = rawsocket_send(&estado->rawsock, pack, tamanho_total);
        
        int tam = tamanho_total + 42;
        //fprintf(stderr,"Tamanho enviado %d oq achamos que seria enviado %d", enviados, tam);

        if (enviados == 42 + tamanho_total) {
           // fprintf(stderr,"Tamanho enviado %d oq achamos que seria enviado %d", enviados, tam );

            return 0;
        }

        if (enviados < 0) {
            fprintf(stderr, "Erro no envio (tentativa %d/%d)\n", 
                   tentativa + 1, MAX_TENTATIVAS);
        }

        sleep(TIMEOUT_SEGUNDOS);
    }

    fprintf(stderr, "Falha ao enviar após %d tentativas\n", MAX_TENTATIVAS);
    return -4;
}

const char *uint8_to_bits(uint8_t num) {
    static char bits[9]; // 8 bits + null terminator
    for (int i = 7; i >= 0; i--) {
        bits[7 - i] = (num & (1 << i)) ? '1' : '0';
    }
    bits[8] = '\0';
    return bits;
}

int receber_pack(estado_protocolo_t* estado, pack_t* pack) {
    if (!estado || !pack) return -1;

    // Configurar timeout no raw socket
    struct timeval timeout = { .tv_sec = TIMEOUT_SEGUNDOS, .tv_usec = 0 };
    if (setsockopt(estado->rawsock.sockfd, SOL_SOCKET, SO_RCVTIMEO, 
                   &timeout, sizeof(timeout)) < 0) {
        perror("Erro ao configurar timeout");
        return -1;
    }

    unsigned int src_ip;
    unsigned short src_port;
    
    int recebidos = rawsocket_receive(&estado->rawsock, pack, sizeof(pack_t), 
                                     &src_ip, &src_port);

    if (recebidos < 0) {
        if (recebidos == -2) {
            return -2; // Timeout
        }
        fprintf(stderr, "Erro no recebimento\n");
        return -1;
    }
    
    if (recebidos == 0) {
        return -2; // Nenhum pacote relevante recebido
    }

    if (recebidos < 4) {
        fprintf(stderr, "Pack muito pequeno recebido: %d bytes\n", recebidos);
        return -1;
    }

    uint8_t tamanho = pack->tamanho;
    if (tamanho > 127 || recebidos != 4 + tamanho) {
        fprintf(stderr, "Tamanho de pack inválido: recebido %d, esperado %u\n", 
               recebidos, 4 + tamanho);
        return -1;
    }
        //fprintf(stderr, "Tamanho de pack válido: recebido %d, esperado %u\n", 
          //     recebidos, 4 + tamanho);

    uint8_t mark = pack->marcador;
    if (mark != 0x7E) {
        fprintf(stderr, "Marcador inválido: recebido %s, esperado %s\n", 
               uint8_to_bits(mark), uint8_to_bits(0x7E));
        return -1;
    }

    // Verificar checksum
    uint8_t checksum_recebido = pack->checksum;
    uint8_t checksum_calculado = calcular_checksum(pack);
    if (checksum_recebido != checksum_calculado) {
        fprintf(stderr, "Checksum inválido: esperado %u, recebido %u\n",
                checksum_calculado, checksum_recebido);
        return -3; // erro de integridade
    }

    // Atualizar informações do remetente (para respostas)
    struct in_addr addr;
    addr.s_addr = src_ip;
    strncpy(estado->dest_ip, inet_ntoa(addr), sizeof(estado->dest_ip) - 1);
    estado->dest_port = src_port;
    
    // Atualizar destino no raw socket
    rawsocket_set_destination(&estado->rawsock, estado->dest_ip, estado->dest_port);

    return 0;
}

int enviar_ack(estado_protocolo_t* estado, uint8_t seq)  {
    pack_t pack;
    criar_pack(&pack, seq, TIPO_ACK, NULL, 0);
    memcpy(&estado->pack, &pack, sizeof(pack_t));
    return enviar_pack(estado, &pack);
}

int enviar_nack(estado_protocolo_t* estado, uint8_t seq) {
    pack_t pack;
    criar_pack(&pack, seq, TIPO_NACK, NULL, 0);
    memcpy(&estado->pack, &pack, sizeof(pack_t));
    return enviar_pack(estado, &pack);
}

int enviar_ok_ack (estado_protocolo_t* estado, uint8_t seq) {
    pack_t pack;
    criar_pack(&pack, seq, TIPO_OK_ACK, NULL, 0);
    memcpy(&estado->pack, &pack, sizeof(pack_t));
    return enviar_pack(estado, &pack);
}

int enviar_erro(estado_protocolo_t* estado, uint8_t seq, tipo_erro_t erro) {
    pack_t pack;
    uint8_t dados_erro = (uint8_t)erro;
    criar_pack(&pack, seq, TIPO_ERRO, &dados_erro, 1);
    memcpy(&estado->pack, &pack, sizeof(pack_t));
    return enviar_pack(estado, &pack);
}

int esperar_ack(estado_protocolo_t* estado) {
    pack_t resposta;

    do{
        int result = receber_pack(estado, &resposta);
        int isSeq = seqCheck(estado->seq_atual, getSeq(resposta));
        if (isSeq == 0){
            if (result < 0) {
                return result;
            }
            if (resposta.tipo == TIPO_OK_ACK){
                return 1;
            } else if (resposta.tipo == TIPO_ACK) {
                return 0;
            } else if (resposta.tipo == TIPO_NACK) {
                return -3; 
            }
        }
        else 
            return -1;
    } while(1);
     // Resposta inesperada
}

void inicializar_jogo(estado_jogo_t* jogo) {

    if (!jogo) return;
    
    // Limpar estruturas
    memset(jogo, 0, sizeof(estado_jogo_t));
    
    // Posição inicial do jogador
    jogo->posicao_jogador.x = 0;
    jogo->posicao_jogador.y = 0;
    jogo->grid_visitado[0][0] = 1;
    
    // Seed para random
    srand(time(NULL));
    
    // Sortear posições dos tesouros
    printf("Sorteando posições dos tesouros...\n");
    
    for (int i = 0; i < NUM_TESOUROS; i++) {
        int x, y;
        int posicao_ocupada;
        
        do {
            x = rand() % TAMANHO_GRID;
            y = rand() % TAMANHO_GRID;
            
            // Verificar se posição já está ocupada
            posicao_ocupada = 0;
            
            // Não colocar tesouro na posição inicial (0,0)
            if (x == 0 && y == 0) {
                posicao_ocupada = 1;
            }
            
            // Verificar outros tesouros
            for (int j = 0; j < i; j++) {
                if (jogo->tesouros[j].posicao.x == x && jogo->tesouros[j].posicao.y == y) {
                    posicao_ocupada = 1;
                    break;
                }
            }
        } while (posicao_ocupada);
        
        jogo->tesouros[i].posicao.x = x;
        jogo->tesouros[i].posicao.y = y;
        jogo->tesouros[i].encontrado = 0;
        
        // Definir nome e tipo do arquivo
        if (i < 3) {
            // Primeiros 3 são textos
            snprintf(jogo->tesouros[i].nome_arquivo, sizeof(jogo->tesouros[i].nome_arquivo),
                     "%d.txt", i + 1);
            jogo->tesouros[i].tipo = TESOURO_TEXTO;
        } else if (i < 6) {
            // Próximos 3 são imagens
            snprintf(jogo->tesouros[i].nome_arquivo, sizeof(jogo->tesouros[i].nome_arquivo),
                     "%d.jpg", i + 1);
            jogo->tesouros[i].tipo = TESOURO_IMAGEM;
        } else {
            // Últimos 2 são vídeos
            snprintf(jogo->tesouros[i].nome_arquivo, sizeof(jogo->tesouros[i].nome_arquivo),
                     "%d.mp4", i + 1);
            jogo->tesouros[i].tipo = TESOURO_VIDEO;
        }
        
        char nome_temp[TAMANHO_MAX_NOME];
        strncpy(nome_temp, jogo->tesouros[i].nome_arquivo, sizeof(nome_temp));
        nome_temp[sizeof(nome_temp) - 1] = '\0';

        snprintf(jogo->tesouros[i].caminho_completo, sizeof(jogo->tesouros[i].caminho_completo),
             "./objetos/%s", nome_temp);

        // Obter tamanho do arquivo
        obter_tamanho_arquivo(jogo->tesouros[i].caminho_completo, jogo->tesouros[i].tamanho);
        
        uint64_t tamanho_arquivo;
        memcpy(&tamanho_arquivo, jogo->tesouros[i].tamanho, sizeof(uint64_t));
        
        printf("Tesouro %d: %s na posição (%d,%d) - %llu bytes\n",
               i + 1, jogo->tesouros[i].nome_arquivo, x, y, 
               (unsigned long long)tamanho_arquivo);
    }
}

int mover_jogador(estado_jogo_t* jogo, tipo_msg_t direcao) {
    if (!jogo) return -1;
    
    posicao_t nova_posicao = jogo->posicao_jogador;
    
    switch (direcao) {
        case TIPO_DESLOCA_DIREITA:
            nova_posicao.x++;
            break;
        case TIPO_DESLOCA_ESQUERDA:
            nova_posicao.x--;
            break;
        case TIPO_DESLOCA_CIMA:
            nova_posicao.y++;
            break;
        case TIPO_DESLOCA_BAIXO:
            nova_posicao.y--;
            break;
        default:
            return -1;
    }
    
    // Verificar limites do grid
    if (nova_posicao.x < 0 || nova_posicao.x >= TAMANHO_GRID ||
        nova_posicao.y < 0 || nova_posicao.y >= TAMANHO_GRID) {
        return -1; // Movimento inválido
    }
    
    // Atualizar posição
    jogo->posicao_jogador = nova_posicao;
    jogo->grid_visitado[nova_posicao.x][nova_posicao.y] = 1;
    
    return 0; // Movimento válido
}

int verificar_tesouro(estado_jogo_t* jogo, posicao_t posicao) {
    if (!jogo) return -1;
    
    for (int i = 0; i < NUM_TESOUROS; i++) {
        if (jogo->tesouros[i].posicao.x == posicao.x &&
            jogo->tesouros[i].posicao.y == posicao.y &&
            !jogo->tesouros[i].encontrado) {
            
            jogo->tesouros[i].encontrado = 1;
            jogo->tesouros_encontrados++;
            return i; // Índice do tesouro encontrado
        }
    }
    
    return -1; // Nenhum tesouro encontrado
}

void mostrar_mapa_servidor(estado_jogo_t* jogo) {
    if (!jogo) return;
    limpar_tela();  
    printf("\n=== MAPA DO SERVIDOR ===\n");
    printf("Posição do jogador: (%d,%d)\n", jogo->posicao_jogador.x, jogo->posicao_jogador.y);
    printf("Tesouros encontrados: %d/%d\n", jogo->tesouros_encontrados, NUM_TESOUROS);
    
    printf("\nLegenda: 🧍 = Jogador, 💎 = Tesouro, ❌ = Tesouro encontrado, 👣 = Visitado, ⬜ = Vazio\n");
    
    // Mostrar grid (y crescente para cima)
    for (int y = TAMANHO_GRID - 1; y >= 0; y--) {
        printf("%d ", y);
        for (int x = 0; x < TAMANHO_GRID; x++) {
            if (jogo->posicao_jogador.x == x && jogo->posicao_jogador.y == y) {
                printf("🧍 ");
            } else {
                // Verificar se há tesouro nesta posição
                int tem_tesouro = 0;
                for (int i = 0; i < NUM_TESOUROS; i++) {
                    if (jogo->tesouros[i].posicao.x == x && jogo->tesouros[i].posicao.y == y) {
                        if (jogo->tesouros[i].encontrado) {
                            printf("❌ ");
                        } else {
                            printf("💎 ");
                        }
                        tem_tesouro = 1;
                        break;
                    }
                }
                
                if (!tem_tesouro) {
                    if (jogo->grid_visitado[x][y]) {
                        printf("👣 ");
                    } else {
                        printf("⬜ ");
                    }
                }
            }
        }
        printf("\n");
    }
    
    printf("  ");
    for (int x = 0; x < TAMANHO_GRID; x++) {
        printf("%d ", x);
    }
    printf("\n========================\n");
}

void obter_tamanho_arquivo(const char* caminho, uint8_t tam[MAX_DADOS]) {
    if (!caminho || !tam) return;
    
    struct stat st;
    uint64_t tamanho = 0;
    
    if (stat(caminho, &st) == 0) {
        tamanho = (uint64_t)st.st_size;
    }
    
    memcpy(tam, &tamanho, sizeof(uint64_t));
}

tipo_msg_t determinar_tipo_arquivo(const char* nome_arquivo) {
    if (!nome_arquivo) return TIPO_TEXTO_ACK_NOME;
    
    const char* extensao = strrchr(nome_arquivo, '.');
    if (!extensao) return TIPO_TEXTO_ACK_NOME;
    
    if (strcasecmp(extensao, ".txt") == 0) {
        return TIPO_TEXTO_ACK_NOME;
    } else if (strcasecmp(extensao, ".jpg") == 0 || strcasecmp(extensao, ".jpeg") == 0) {
        return TIPO_IMAGEM_ACK_NOME;
    } else if (strcasecmp(extensao, ".mp4") == 0 || strcasecmp(extensao, ".mp3") == 0) {
        return TIPO_VIDEO_ACK_NOME;
    }
    
    return TIPO_TEXTO_ACK_NOME;
}

uint64_t obter_espaco_livre(const char* caminho) {
    struct statvfs info;

    if (statvfs(caminho, &info) != 0) {
        perror("Erro ao obter informações do sistema de arquivos");
        return 0;
    }

    return (uint64_t) info.f_bsize * info.f_bavail;
}
