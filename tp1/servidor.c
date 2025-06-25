#include "protocolo.h"

#define PORTA_SERVIDOR 50969

// Variáveis globais
estado_protocolo_t estado_servidor;
estado_jogo_t jogo;

// Protótipos das funções
int inicializar_servidor();
int processar_mensagem_cliente();
int processar_movimento(tipo_msg_t direcao);
int enviar_mapa_cliente();
int enviar_tesouro(int indice_tesouro);
int enviar_arquivo_tesouro(const char* caminho_arquivo, const char* nome_arquivo, tipo_msg_t tipo);
void log_movimento(const char* direcao, int sucesso);

int main() {
    printf("=== SERVIDOR CAÇA AO TESOURO ===\n");
    
    // Inicializar servidor
    if (inicializar_servidor() < 0) {
        fprintf(stderr, "Erro ao inicializar servidor\n");
        return 1;
    }
    
    printf("Servidor iniciado na porta %d\n", PORTA_SERVIDOR);
    
    // Inicializar jogo
    inicializar_jogo(&jogo);
    mostrar_mapa_servidor(&jogo);
    
    printf("\nAguardando conexão do cliente...\n");
    
    // Loop principal do servidor
    while (1) {
        if (processar_mensagem_cliente() < 0) {
            continue; // Continuar aguardando próxima mensagem
        }
        
        // Verificar se jogo terminou
        if (jogo.tesouros_encontrados >= NUM_TESOUROS) {
            printf("\n🎉 JOGO CONCLUÍDO! Todos os tesouros foram encontrados!\n");
            
            printf("Reiniciando jogo...\n");
            inicializar_jogo(&jogo);
            mostrar_mapa_servidor(&jogo);
        }
    }
    
    close(estado_servidor.socket_fd);
    return 0;
}

int inicializar_servidor() {
    // Criar socket UDP
    estado_servidor.socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (estado_servidor.socket_fd < 0) {
        perror("Erro ao criar socket");
        return -1;
    }
    
    // Configurar endereço do servidor
    struct sockaddr_in endereco_servidor;
    memset(&endereco_servidor, 0, sizeof(endereco_servidor));
    endereco_servidor.sin_family = AF_INET;
    endereco_servidor.sin_addr.s_addr = INADDR_ANY;
    endereco_servidor.sin_port = htons(PORTA_SERVIDOR);
    
    // Bind do socket
    if (bind(estado_servidor.socket_fd, (struct sockaddr*)&endereco_servidor, 
             sizeof(endereco_servidor)) < 0) {
        perror("Erro no bind");
        close(estado_servidor.socket_fd);
        return -1;
    }
    
    // Inicializar estado
    estado_servidor.seq_atual = 0;
    estado_servidor.seq_esperada = 0;
    estado_servidor.tamanho_endereco = sizeof(estado_servidor.endereco_remoto);
    
    return 0;
}



int processar_mensagem_cliente() {
    pack_t pack;
    
    // Receber frame do cliente
    int result = receber_pack(&estado_servidor, &pack);
    if (result < 0) {
        if (result == -2) {
            // Timeout normal
            return 0;
        }
        return -1;
    }
    
    // Processar mensagem baseado no tipo
    switch (pack.tipo) {
        case TIPO_INICIAR_JOGO:
            printf("Cliente solicitou início do jogo\n");
            jogo.jogo_iniciado = 1;
        
            // Enviar ACK com a mesma sequência recebida
            if (enviar_ack(&estado_servidor, getSeq(pack)) < 0) {
                printf("Erro ao enviar ACK\n");
                return -1;
            }
            while(1){
                // Enviar mapa inicial com nova sequência
                estado_servidor.seq_atual = getSeq(pack) + 1 % 32;
                printf("DEBUG: Enviando mapa com seq=%d\n", estado_servidor.seq_atual);
                enviar_mapa_cliente();
                if(esperar_ack(&estado_servidor) < 0){
                    continue;
                }
                else
                    break;
            }
            return 1;
            
        // ... resto do código permanece igual ...
        case TIPO_DESLOCA_DIREITA:
            return processar_movimento(TIPO_DESLOCA_DIREITA);
            
        case TIPO_DESLOCA_ESQUERDA:
            return processar_movimento(TIPO_DESLOCA_ESQUERDA);
            
        case TIPO_DESLOCA_CIMA:
            return processar_movimento(TIPO_DESLOCA_CIMA);
            
        case TIPO_DESLOCA_BAIXO:
            return processar_movimento(TIPO_DESLOCA_BAIXO);
            
        default:
            printf("Tipo de mensagem não reconhecido: %d\n", pack.tipo);
            return enviar_erro(&estado_servidor, (pack.seq_bit0 | (pack.seq_bits1_4 << 1)), ERRO_SEM_PERMISSAO);
    }
    return 1;
}



int processar_movimento(tipo_msg_t direcao) {
    const char* nome_direcao;
    switch (direcao) {
        case TIPO_DESLOCA_DIREITA: nome_direcao = "DIREITA"; break;
        case TIPO_DESLOCA_ESQUERDA: nome_direcao = "ESQUERDA"; break;
        case TIPO_DESLOCA_CIMA: nome_direcao = "CIMA"; break;
        case TIPO_DESLOCA_BAIXO: nome_direcao = "BAIXO"; break;
        default: nome_direcao = "DESCONHECIDA"; break;
    }
    
    // Tentar mover jogador
    if (mover_jogador(&jogo, direcao) < 0) {
        printf("❌ Movimento %s inválido - posição atual: (%d,%d)\n", 
               nome_direcao, jogo.posicao_jogador.x, jogo.posicao_jogador.y);
        log_movimento(nome_direcao, 0);
        
        // Enviar erro de movimento inválido
        return enviar_erro(&estado_servidor, estado_servidor.seq_atual, ERRO_MOVIMENTO_INVALIDO);
    }
    
    printf("✅ Movimento %s realizado - nova posição: (%d,%d)\n", 
           nome_direcao, jogo.posicao_jogador.x, jogo.posicao_jogador.y);
    log_movimento(nome_direcao, 1);
    
    // Mostrar mapa atualizado
    mostrar_mapa_servidor(&jogo);
    
    // Verificar se há tesouro na nova posição
    int indice_tesouro = verificar_tesouro(&jogo, jogo.posicao_jogador);
    if (indice_tesouro >= 0) {
        printf("🏆 TESOURO ENCONTRADO! %s na posição (%d,%d)\n", 
               jogo.tesouros[indice_tesouro].nome_arquivo,
               jogo.posicao_jogador.x, jogo.posicao_jogador.y);
        while(1){
            enviar_mapa_cliente();
            if(esperar_ack(&estado_servidor) < 0){
                continue;
            }
            else
                break;
        }
        enviar_tesouro(indice_tesouro);
        return 1;
    } else {
        while(1){
        // Apenas enviar nova posição
        enviar_mapa_cliente();
        if(esperar_ack(&estado_servidor) < 0)
                continue;
        else
            break;
        }
        return 1;
    }
}


int searchTreasure(tesouro_t treasures[NUM_TESOUROS], posicao_t pos){
    for(int i = 0; i < NUM_TESOUROS; i++){
        if((treasures[i].posicao.x == pos.x)&&
                (treasures[i].posicao.y == pos.y)){
            return 1;
        }
    }
    return 0;
}


int enviar_mapa_cliente() {
    pack_t pack;
    frame_map_t mapa_dados;
    
    // Preparar dados do mapa para o cliente
    mapa_dados.playerPos = jogo.posicao_jogador;
    
    //confere se achou um tesouro
    mapa_dados.findTreasure = searchTreasure(jogo.tesouros, mapa_dados.playerPos);

    criar_pack(&pack, estado_servidor.seq_atual, TIPO_MAPA_CLIENTE, 
                (uint8_t*)&mapa_dados, sizeof(mapa_dados));
    
    printf("tipo do mapa %d deveria ser %d achou tesouro: %d\n", pack.tipo, TIPO_MAPA_CLIENTE, mapa_dados.findTreasure);
    
    return enviar_pack(&estado_servidor, &pack);
}


int enviar_tesouro(int indice_tesouro) {
    tesouro_t* tesouro = &jogo.tesouros[indice_tesouro];
    
    // Cria o pack
    pack_t pack;
    
    if (criar_pack(&pack, estado_servidor.seq_atual, TIPO_TAMANHO,
                   tesouro->tamanho, sizeof(tesouro->tamanho)) < 0) {
        fprintf(stderr, "Erro ao criar pack do tesouro\n");
        return -1;
    }
    while(1){
        // Envia o pack
        if (enviar_pack(&estado_servidor, &pack) < 0)
            continue;

        // Aguardar ACK
        if (esperar_ack(&estado_servidor) < 0)
            continue;
        else{
            while(1){
                uint64_t tamanho_lido;
                memcpy(&tamanho_lido, tesouro->tamanho, sizeof(uint64_t));
                printf("Tamanho do arquivo = %llu bytes\n", (unsigned long long)tamanho_lido);

                break;
            }

            break;
        }
    }

    // Determinar tipo do arquivo
    tipo_msg_t tipo = determinar_tipo_arquivo(tesouro->nome_arquivo);

    enviar_arquivo_tesouro(tesouro->caminho_completo, tesouro->nome_arquivo, tipo);
    return 0;
}

int enviar_arquivo_tesouro(const char* caminho_arquivo, const char* nome_arquivo, tipo_msg_t tipo) {

    FILE* arquivo = fopen(caminho_arquivo, "rb");
    if (!arquivo) {
        perror("Erro ao abrir arquivo do tesouro");
        return -1;
    }

    printf("Nome do arquivo: %s\n", nome_arquivo);



    // Primeiro enviar o nome do arquivo
    pack_t pack_nome;
    estado_servidor.seq_atual = (estado_servidor.seq_atual + 1) % 32;
    criar_pack(&pack_nome, estado_servidor.seq_atual, tipo, 
              (uint8_t*)nome_arquivo, strlen(nome_arquivo) + 1);
    
    while(1) {
        if (enviar_pack(&estado_servidor, &pack_nome) < 0) {
            continue;
        }
        if (esperar_ack(&estado_servidor) < 0) {
            continue;
        }
        break;
    }

    
    // Enviar o arquivo em chunks
    uint8_t buffer[MAX_DADOS];
    size_t bytes_lidos;
    memset(buffer, 0, sizeof(buffer));

    while ((bytes_lidos = fread(buffer, 1, MAX_DADOS, arquivo)) > 0) {
        pack_t pack_dados;
        estado_servidor.seq_atual = (estado_servidor.seq_atual + 1) % 32;
               
        criar_pack(&pack_dados, estado_servidor.seq_atual, TIPO_DADOS, buffer, bytes_lidos);
        
        while(1) {
            if (enviar_pack(&estado_servidor, &pack_dados) < 0) {
                bytes_lidos = 0;
                memset(buffer, 0, sizeof(buffer));
                continue;
            }
            if (esperar_ack(&estado_servidor) < 0) {
                bytes_lidos = 0;
                memset(buffer, 0, sizeof(buffer));
                continue;
            }
            break;
        }
        bytes_lidos = 0;
        memset(buffer, 0, sizeof(buffer));

    }

    fclose(arquivo);
    return 0;
}

void log_movimento(const char* direcao, int sucesso) {
    time_t now = time(NULL);
    char* time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0'; // Remover \n
    
    printf("[%s] Movimento %s: %s - Posição: (%d,%d) - Tesouros: %d/%d\n",
           time_str, direcao, sucesso ? "OK" : "INVÁLIDO",
           jogo.posicao_jogador.x, jogo.posicao_jogador.y,
           jogo.tesouros_encontrados, NUM_TESOUROS);
}
