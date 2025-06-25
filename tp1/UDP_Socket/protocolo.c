#include "protocolo.h"


int enviar_ack(estado_protocolo_t* estado, uint8_t seq)  {
    pack_t pack;
    criar_pack(&pack, seq, TIPO_ACK, NULL, 0);
    return enviar_pack(estado, &pack);
}

int enviar_nack(estado_protocolo_t* estado, uint8_t seq) {
    pack_t pack;
    criar_pack(&pack, seq, TIPO_NACK, NULL, 0);
    return enviar_pack(estado, &pack);
}

int enviar_ok_ack (estado_protocolo_t* estado, uint8_t seq) {
    pack_t pack;
    criar_pack(&pack, seq, TIPO_NACK, NULL, 0);
    return enviar_pack(estado, &pack);
}

int enviar_erro(estado_protocolo_t* estado, uint8_t seq, tipo_erro_t erro) {
    pack_t pack;
    uint8_t dados_erro = (uint8_t)erro;
    criar_pack(&pack, seq, TIPO_ERRO, &dados_erro, 1);
    return enviar_pack(estado, &pack);
}

int esperar_ack(estado_protocolo_t* estado) {
    pack_t resposta;

    int result = receber_pack(estado, &resposta);
    if (result < 0) {
        return result;
    }

    if (resposta.tipo == TIPO_ACK) {
        return 0;
    } else if (resposta.tipo == TIPO_NACK) {
        return -3; // NACK recebido
    } else if (resposta.tipo == TIPO_ERRO) {
        return -4; // Erro recebido
    }

    return -1; // Resposta inesperada
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
/*
        // Caminho completo
        snprintf(jogo->tesouros[i].caminho_completo, sizeof(jogo->tesouros[i].caminho_completo),
                 "./objetos/%s", jogo->tesouros[i].nome_arquivo);*/
        
        // Obter tamanho do arquivo
        obter_tamanho_arquivo(jogo->tesouros[i].caminho_completo, jogo->tesouros[i].tamanho);
        
        printf("Tesouro %d: %s na posição (%d,%d) - %u bytes\n",
               i + 1, jogo->tesouros[i].nome_arquivo, x, y, *(jogo->tesouros[i].tamanho));
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

void limpar_tela() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
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
    if (!caminho) return ;
    
    struct stat st;
    if (stat(caminho, &st) == 0) {
        memcpy(tam, (uint64_t *)&st.st_size, sizeof(uint64_t)); // Copia 8 bytes
        return;
    }
    return;
}

unsigned long obter_espaco_livre(const char* diretorio) {
    if (!diretorio) return 0;
    
    struct statvfs st;
    if (statvfs(diretorio, &st) == 0) {
        return st.f_bavail * st.f_frsize;
    }
    return 0;
}

int is_arquivo_regular(const char* caminho) {
    if (!caminho) return 0;
    
    struct stat st;
    if (stat(caminho, &st) == 0) {
        return S_ISREG(st.st_mode);
    }
    return 0;
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






uint8_t getSeq(pack_t pack){
    return pack.seq_bit0 | (pack.seq_bits1_4 << 1);
}







// --- NOVO: Cálculo de checksum ---
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

    if (dados && tamanho > 0) {
        memcpy(pack->dados, dados, tamanho);
    }

    // --- NOVO: aplicar checksum real ---
    pack->checksum = calcular_checksum(pack);

    return 0;
}

int enviar_pack(estado_protocolo_t* estado, const pack_t* pack) {
    if (!estado || !pack) return -1;



    int tamanho_total = 4 + pack->tamanho;

    for (int tentativa = 0; tentativa < MAX_TENTATIVAS; tentativa++) {
        ssize_t enviados = sendto(estado->socket_fd, pack, tamanho_total, 0,
                                  (struct sockaddr*)&estado->endereco_remoto,
                                  estado->tamanho_endereco);

        if (enviados == tamanho_total) return 0;

        if (enviados < 0) perror("Erro no sendto");

        sleep(TIMEOUT_SEGUNDOS);
    }

    return -1;
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

    struct timeval timeout = { .tv_sec = TIMEOUT_SEGUNDOS, .tv_usec = 0 };

    if (setsockopt(estado->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Erro ao configurar timeout");
        return -1;
    }

    ssize_t recebidos = recvfrom(estado->socket_fd, pack, sizeof(pack_t), 0,
                                 (struct sockaddr*)&estado->endereco_remoto,
                                 &estado->tamanho_endereco);

    if (recebidos < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return -2; // Timeout
        perror("Erro no recvfrom");
        return -1;
    }

    if (recebidos < 4) {
        fprintf(stderr, "Pack muito pequeno recebido\n");
        return -1;
    }

    uint8_t tamanho = pack->tamanho;
    if (tamanho > 127 || recebidos != 4 + tamanho) {
        fprintf(stderr, "Tamanho de pack inválido recebido %zd esperado %u\n", recebidos, 4 + tamanho);
        sleep(TIMEOUT_SEGUNDOS);
        return -1;
    }

    uint8_t mark = pack->marcador;
    if (mark != 126){
        fprintf(stderr, "Marcador de inicio do pack inválido recebido %s esperado %s\n", uint8_to_bits(mark), uint8_to_bits(126));
        return -1;
    }

    // --- NOVO: verificar checksum ---
    uint8_t checksum_recebido = pack->checksum;
    uint8_t checksum_calculado = calcular_checksum(pack);
    if (checksum_recebido != checksum_calculado) {
        fprintf(stderr, "Checksum inválido: esperado %u, recebido %u\n",
                checksum_calculado, checksum_recebido);
        return -3; // erro de integridade
    }

    return 0;
}


