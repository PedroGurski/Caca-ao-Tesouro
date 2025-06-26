#include "protocolo.h"
#include "rawSocket.h"

// Variáveis globais
estado_protocolo_t estado_servidor;
estado_jogo_t jogo;

// Protótipos das funções
int inicializar_servidor();
int conectar_cliente(const char* ip_servidor, int porta);
int processar_mensagem_cliente();
int processar_movimento(tipo_msg_t direcao);
int enviar_mapa_cliente();
int enviar_tesouro(int indice_tesouro);
int enviar_arquivo_tesouro(const char* caminho_arquivo, const char* nome_arquivo, tipo_msg_t tipo);
void log_movimento(const char* direcao, int sucesso);
int searchTreasure(tesouro_t treasures[NUM_TESOUROS], posicao_t pos);

int main(int argc, char* argv[]) {
    int porta_servidor = PORTA_C;
    char ip_servidor[16];

    printf("=== SERVIDOR CAÇA AO TESOURO ===\n");
    
    // Obter IP do servidor
    if (argc > 1) {
        strcpy(ip_servidor, argv[1]);
    } else {
        printf("Digite o IP do cliente: ");
        if (scanf("%15s", ip_servidor) != 1) {
            fprintf(stderr, "Erro ao ler IP do cliente\n");
            return 1;
        }
    }


        // Conectar ao servidor
    if (conectar_cliente(ip_servidor, porta_servidor) < 0) {
        fprintf(stderr, "Erro ao conectar ao cliente\n");
        return 1;
    }
    
    printf("Servidor iniciado na porta %d\n", PORTA_S);
    
    // Inicializar jogo    
    inicializar_jogo(&jogo);
    mostrar_mapa_servidor(&jogo);
    
    printf("\nAguardando conexão do cliente...\n");
    
    // Loop principal do servidor
    while (1) {
        int p = processar_mensagem_cliente();
        if (p == -4){
            finalizar_protocolo(&estado_servidor);
            limpar_tela();
            perror("=========Erro fatal!!! Digite ENTER para matar o programa==========\n");
            getchar();
            return 0;
        }
        if (p < 0) {
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
    
    finalizar_protocolo(&estado_servidor);
    return 0;
}

int conectar_cliente(const char* ip_servidor, int porta) {
    const char* interface = INTERFACE_NAME;
    // Inicializar protocolo com raw socket
    if (inicializar_protocolo(&estado_servidor, ip_servidor, PORTA_S, porta, interface) < 0) {
        fprintf(stderr, "Erro ao inicializar protocolo cliente\n");
        return -1;
    }
    
    return 0;
}

//processa as mensagens recebidas pelo cliente
int processar_mensagem_cliente() {
    pack_t pack;
    
    // Receber frame do cliente
    int result = receber_pack(&estado_servidor, &pack);
    if(jogo.jogo_iniciado == 1){
        int isSeq = seqCheck(estado_servidor.seq_atual , getSeq(pack));
        switch(isSeq){
            case 1:
                break;
            default:
                if((pack.tipo == TIPO_ACK)||(pack.tipo == TIPO_NACK)||(pack.tipo == TIPO_OK_ACK))
                    return 0;
                //fprintf(stderr, "Seq Esperado %u Seq recebido %u\n", ((estado_servidor.seq_atual+1)%32), getSeq(pack));
                //estado_servidor.seq_atual = getSeq(pack);

                return reenvio(&estado_servidor, estado_servidor.pack);
        }

        if (result < 0) {
            if (result == -2) {
                // Timeout normal
                return 0;
            }
            return -1;
        }


      //  printf("RECEBIDO SEQ: %d \n", getSeq(pack));
    //    sleep(2);
        estado_servidor.seq_atual = getSeq(pack);
  //      printf("ATUAL SEQ: %d \n", estado_servidor.seq_atual);
//        sleep(2);

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
                estado_servidor.seq_atual = (getSeq(pack) + 1) % 32;

                if (enviar_mapa_cliente() < 0) {
                    sleep(TIMEOUT_SEGUNDOS);
                    continue;
                }
                if(esperar_ack(&estado_servidor) < 0){
                    continue;
                }
                else
                    break;
            }
            return 1;
            
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
            return enviar_erro(&estado_servidor, getSeq(pack), ERRO_SEM_PERMISSAO);
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
        return enviar_ack(&estado_servidor, estado_servidor.seq_atual);
    }
    
    printf("✅ Movimento %s realizado - nova posição: (%d,%d)\n", 
           nome_direcao, jogo.posicao_jogador.x, jogo.posicao_jogador.y);
    log_movimento(nome_direcao, 1);

    // Mostrar mapa atualizado
    mostrar_mapa_servidor(&jogo);

    if (enviar_ok_ack(&estado_servidor, estado_servidor.seq_atual) < 0) {
        return -1;
    }
    
    // Verificar se há tesouro na nova posição
    int indice_tesouro = verificar_tesouro(&jogo, jogo.posicao_jogador);
    if (indice_tesouro >= 0) {
        printf("🏆 TESOURO ENCONTRADO! %s na posição (%d,%d)\n", 
               jogo.tesouros[indice_tesouro].nome_arquivo,
               jogo.posicao_jogador.x, jogo.posicao_jogador.y);
        int i = 0;
        estado_servidor.seq_atual = (estado_servidor.seq_atual + 1) % 32;
        while(1){

            if(enviar_mapa_cliente() == -4)
                return -4;
            if(esperar_ack(&estado_servidor) >= 0){
                break;
            }
            else if(i >= MAX_TENTATIVAS)
                continue;
            i += 1;
        }
        estado_servidor.seq_atual = (estado_servidor.seq_atual + 1) % 32;
        return enviar_tesouro(indice_tesouro);
    } else {
        int j = 0;
        estado_servidor.seq_atual = (estado_servidor.seq_atual + 1) % 32;
        while(1){


            // Apenas enviar nova posição


            if (enviar_mapa_cliente() < 0) {
                sleep(TIMEOUT_SEGUNDOS);
                continue;
            }
            if(esperar_ack(&estado_servidor) >= 0)
                break;
            else if(j >= MAX_TENTATIVAS)
                continue;
            j += 1;
        }
        return 1;
    }
}

//procura se existe um tesouro na posição
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

    if (criar_pack(&pack, estado_servidor.seq_atual, TIPO_MAPA_CLIENTE, 
                (uint8_t*)&mapa_dados, sizeof(mapa_dados)) < 0) {
        return -1;
    }

    memcpy(&estado_servidor.pack, &pack, sizeof(pack_t));    
    return enviar_pack(&estado_servidor, &pack);
}

int enviar_tesouro(int indice_tesouro) {
    tesouro_t* tesouro = &jogo.tesouros[indice_tesouro];
    
    // Cria o pack
    pack_t pack;
    
    while(1){
        if (criar_pack(&pack, estado_servidor.seq_atual, TIPO_TAMANHO,
                       tesouro->tamanho, sizeof(tesouro->tamanho)) < 0) {
            fprintf(stderr, "Erro ao criar pack do tesouro\n");
            continue;
        }
        break;
    }
    while(1){
        printf("ENVIANDO TAMANHO TESOURO\n");
        // Envia o pack
        if (enviar_pack(&estado_servidor, &pack) < 0){
            continue;
        }


        // Aguardar ACK
        int typeAck = esperar_ack(&estado_servidor);
        if (typeAck == -4){
            continue;
//            printf("erro fatal\n");
//            return -4;  
        }
        else if(typeAck < 0){
            continue;
        }
        else{
            uint64_t tamanho_lido;
            memcpy(&tamanho_lido, tesouro->tamanho, sizeof(uint64_t));
            printf("Tamanho do arquivo = %llu bytes\n", (unsigned long long)tamanho_lido);
            break;
        }
    }

    // Determinar tipo do arquivo
    tipo_msg_t tipo = determinar_tipo_arquivo(tesouro->nome_arquivo);

    return enviar_arquivo_tesouro(tesouro->caminho_completo, tesouro->nome_arquivo, tipo);
}

int enviar_arquivo_tesouro(const char* caminho_arquivo, const char* nome_arquivo, tipo_msg_t tipo) {

    FILE* arquivo = fopen(caminho_arquivo, "rb");
    if (!arquivo) {
        enviar_erro(&estado_servidor, estado_servidor.seq_atual, ERRO_SEM_PERMISSAO);
        fprintf(stderr, "Erro ao abrir arquivo do tesouro %s \n", nome_arquivo);
        return -1;
    }

    printf("Nome do arquivo: %s\n", nome_arquivo);

    // Primeiro enviar o nome do arquivo
    pack_t pack_nome;
    estado_servidor.seq_atual = (estado_servidor.seq_atual + 1) % 32;
    if (criar_pack(&pack_nome, estado_servidor.seq_atual, tipo, 
              (uint8_t*)nome_arquivo, strlen(nome_arquivo) + 1) < 0) {
        fclose(arquivo);
        return -1;
    }
    
    while(1) {
        if (enviar_pack(&estado_servidor, &pack_nome) < 0) {
            sleep(TIMEOUT_SEGUNDOS);
            continue;
        }
        if (esperar_ack(&estado_servidor) < 0) {
            sleep(TIMEOUT_SEGUNDOS);
            continue;
        }
        break;
    }

    // Enviar o arquivo em chunks
    uint8_t buffer[MAX_DADOS];
    size_t bytes_lidos;
    size_t bytes_enviados = 0;
    
    while ((bytes_lidos = fread(buffer, 1, MAX_DADOS, arquivo)) > 0) {
        pack_t pack_dados;
        while(1){
            int seqTemp = (estado_servidor.seq_atual + 1) % 32;

               
            if (criar_pack(&pack_dados, seqTemp, TIPO_DADOS, buffer, bytes_lidos) < 0) {
                continue;
            }

            estado_servidor.seq_atual = seqTemp;
            break;
        }

        while(1) {
            printf("Bytes enviados %zu\n", bytes_enviados);
            if (enviar_pack(&estado_servidor, &pack_dados) < 0) {
                continue;
            }
            if (esperar_ack(&estado_servidor) < 0) {
                continue;
            }
            break;
        }
        bytes_enviados += bytes_lidos;
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
