#include "protocolo.h"

#define DIRETORIO_TESOUROS "./downloads/"

// Protótipos das funções
int conectar_servidor(estado_cliente_t* cliente, const char* ip_servidor, int porta);
int iniciar_jogo(estado_cliente_t* cliente);
void mostrar_mapa_cliente(estado_cliente_t* cliente);
void mostrar_menu_movimento();
int processar_comando_movimento(estado_cliente_t* cliente, char comando);
int enviar_movimento(estado_cliente_t* cliente, tipo_msg_t tipo_movimento);
int processar_resposta_servidor(estado_cliente_t* cliente);
int receber_tesouro(estado_cliente_t* cliente);
int receber_arquivo_tesouro(estado_cliente_t* cliente, const char* nome_arquivo, tipo_msg_t tipo, uint64_t tamanho);
void exibir_tesouro(const char* nome_arquivo, const char* caminho_completo, tipo_msg_t tipo);

void limpar_tela();
const char* obter_nome_direcao(tipo_msg_t tipo);


// Configura o terminal para modo raw (sem buffer)
void configurar_terminal(struct termios *orig) {
    tcgetattr(STDIN_FILENO, orig);
    struct termios raw = *orig;
    raw.c_lflag &= ~(ICANON | ECHO);  // Desativa buffer de linha e eco
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}
void restaurar_terminal(struct termios *orig) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig);  // Volta ao original
}




int main(int argc, char* argv[]) {
    struct termios orig_termios;  // Variável global para guardar o estado original
    configurar_terminal(&orig_termios);
    estado_cliente_t cliente;
    char ip_servidor[16];
    int porta_servidor = 50969;
    
    // Limpar estrutura do cliente
    memset(&cliente, 0, sizeof(cliente));
    
    printf("=== CLIENTE CAÇA AO TESOURO ===\n");
    
    // Obter IP do servidor
    if (argc > 1) {
        strcpy(ip_servidor, argv[1]);
    } else {
        printf("Digite o IP do servidor: ");
        if (scanf("%15s", ip_servidor) != 1) {
            fprintf(stderr, "Erro ao ler IP do servidor\n");
            return 1;
        }
    }
    
    // Conectar ao servidor
    if (conectar_servidor(&cliente, ip_servidor, porta_servidor) < 0) {
        fprintf(stderr, "Erro ao conectar ao servidor\n");
        return 1;
    }
    
    printf("Conectado ao servidor %s:%d\n", ip_servidor, porta_servidor);
    
    // Criar diretório de tesouros se não existir
    system("mkdir -p " DIRETORIO_TESOUROS);
    
    // Iniciar jogo
    if (iniciar_jogo(&cliente) < 0) {
        fprintf(stderr, "Erro ao iniciar jogo\n");
        close(cliente.protocolo.socket_fd);
        return 1;
    }
    
    printf("🎮 Jogo iniciado! Encontre todos os tesouros escondidos no mapa 8x8!\n");
    printf("Pressione ENTER para continuar...");
    getchar(); // Aguardar ENTER

    // Loop principal do jogo
    while (cliente.jogo_ativo) {
        limpar_tela();
        mostrar_mapa_cliente(&cliente);
        mostrar_menu_movimento();
        
        char comando;
        printf("Digite sua escolha: ");        
        int entrada = getchar();
    
        // Converte para char se não for EOF
        if (entrada != EOF) 
            comando = (char)entrada; 
        else
            continue;
        
        if (comando == 'q') {
            printf("Saindo do jogo...\n");
            break;
        }
        
        if (processar_comando_movimento(&cliente, comando) < 0)
            continue;
        if(cliente.mapa_atual.numTreasures == NUM_TESOUROS)
            cliente.jogo_ativo = 0;
    }
    limpar_tela();
    close(cliente.protocolo.socket_fd);
    printf("Obrigado por jogar! 🎉\n Pressione ENTER pra matar o programa.\n");
    getchar();

    restaurar_terminal(&orig_termios);
    return 0;
}

int conectar_servidor(estado_cliente_t* cliente, const char* ip_servidor, int porta) {
    // Criar socket UDP
    cliente->protocolo.socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (cliente->protocolo.socket_fd < 0) {
        perror("Erro ao criar socket");
        return -1;
    }
    
    // Configurar endereço do servidor
    memset(&cliente->protocolo.endereco_remoto, 0, sizeof(cliente->protocolo.endereco_remoto));
    cliente->protocolo.endereco_remoto.sin_family = AF_INET;
    cliente->protocolo.endereco_remoto.sin_port = htons(porta);
    
    if (inet_pton(AF_INET, ip_servidor, &cliente->protocolo.endereco_remoto.sin_addr) <= 0) {
        fprintf(stderr, "Endereço IP inválido: %s\n", ip_servidor);
        close(cliente->protocolo.socket_fd);
        return -1;
    }
    
    // Inicializar estado do protocolo
    cliente->protocolo.seq_atual = 0;
    cliente->protocolo.seq_esperada = 0;
    cliente->protocolo.tamanho_endereco = sizeof(cliente->protocolo.endereco_remoto);
    
    return 0;
}

int searchTreasure(posicao_t colletTreasures[NUM_TESOUROS], int numTreasures, int x, int y){
    for(int i = 0; i < numTreasures; i++){
        if((colletTreasures[i].x == x)&&
                (colletTreasures[i].y == y)){
            return 1;
        }
    }
    return 0;
}

int insertTreasure(mapa_cliente_t *clientMap){
    if(!searchTreasure(clientMap->colletTreasures,
                clientMap->numTreasures, clientMap->playerPos.x, clientMap->playerPos.y)){
        clientMap->colletTreasures[clientMap->numTreasures].x = clientMap->playerPos.x;
        clientMap->colletTreasures[clientMap->numTreasures].y = clientMap->playerPos.y;
        ++(clientMap->numTreasures);
        return 1;
    }
    return 0;
}

void attMap(mapa_cliente_t *clientMap, frame_map_t frameMap, int *newTreasure){

    clientMap->playerPos.x =frameMap.playerPos.x;
    clientMap->playerPos.y =frameMap.playerPos.y;

    if(frameMap.findTreasure){
        *newTreasure = insertTreasure(clientMap);
    }
    else
        clientMap->grid_visitado[clientMap->playerPos.x][clientMap->playerPos.y] = 1;
}

int iniciar_jogo(estado_cliente_t* cliente) {
    // Enviar comando para iniciar jogo
    pack_t frame_inicio;
    criar_pack(&frame_inicio, cliente->protocolo.seq_atual, TIPO_INICIAR_JOGO, NULL, 0);
    
    for(int i = 0; i < TAMANHO_GRID; i++){
        for(int j=0; j < TAMANHO_GRID; j++){
            cliente->mapa_atual.grid_visitado[i][j] = 0;
        }
    }

    while(1){
        if (enviar_pack(&cliente->protocolo, &frame_inicio) < 0) {
            printf("Erro ao enviar comando de início\n");
            continue;
        }
        else
            printf("Comando de inicio enviado\n");

        // Aguardar ACK do servidor
        if (esperar_ack(&cliente->protocolo) < 0) {
            printf("Servidor não confirmou início do jogo\n");
            continue;
        }
        else{
            printf("Servidor confirmou início do jogo\n");
            break;
        }
    }

    while(1){
        // Receber mapa inicial
        pack_t frameRecebido;
        if (receber_pack(&cliente->protocolo, &frameRecebido) < 0) {
            printf("Erro ao receber mapa inicial\n");
            enviar_nack(&cliente->protocolo, getSeq(frameRecebido));
            continue;
        }
        else{
            printf("Recebido mapa inicial\n");
            enviar_ack(&cliente->protocolo, getSeq(frameRecebido));
        }
        if (frameRecebido.tipo != TIPO_MAPA_CLIENTE) {
            printf("Resposta inesperada do servidor: tipo %d\n", frameRecebido.tipo);
            continue;
        }
        frame_map_t frameMapa;
        memcpy(&frameMapa, frameRecebido.dados, sizeof(frame_map_t));

        // Copiar dados do mapa
        cliente->mapa_atual.numTreasures = 0;
        attMap(&cliente->mapa_atual, frameMapa, 0); 
        cliente->jogo_ativo = 1;
        cliente->tesouros_coletados = cliente->mapa_atual.numTreasures;
        break;
    }

    return 0;
}

void mostrar_mapa_cliente(estado_cliente_t* cliente) {
    printf("\n=== MAPA DO CAÇA AO TESOURO ===\n");
    printf("Posição atual: (%d,%d)\n", 
        cliente->mapa_atual.playerPos.x, 
           cliente->mapa_atual.playerPos.y);
    printf("Tesouros encontrados: %d/8\n", cliente->mapa_atual.numTreasures);
    
    printf("\nLegenda: 🧍 = Você, 💎 = Tesouro encontrado, 👣 = Visitado, ⬜ = Inexplorado\n");
    printf("────────────────────────────────────────\n");
    
    // Mostrar grid (y crescente para cima)
    for (int y = TAMANHO_GRID - 1; y >= 0; y--) {
        printf("%d │", y);
        for (int x = 0; x < TAMANHO_GRID; x++) {
            if (cliente->mapa_atual.playerPos.x == x && 
                cliente->mapa_atual.playerPos.y == y) {
                printf(" 🧍");
            } else if (searchTreasure(cliente->mapa_atual.colletTreasures, cliente->mapa_atual.numTreasures, x, y)) {
                printf(" 💎");
            } else if (cliente->mapa_atual.grid_visitado[x][y]) {
                printf(" 👣");
            } else {
                printf(" ⬜");
            }
        }
        printf("\n");
    }
    
    printf("  └");
    for (int i = 0; i < TAMANHO_GRID; i++) {
        printf("───");
    }
    printf("\n   ");
    for (int x = 0; x < TAMANHO_GRID; x++) {
        printf(" %d ", x);
    }
    printf("\n────────────────────────────────────────\n");
}

void mostrar_menu_movimento() {
    printf("\n=== MOVIMENTOS DISPONÍVEIS ===\n");
    printf("w. ⬆️  Mover para CIMA\n");
    printf("a. ⬇️  Mover para BAIXO\n");
    printf("s. ⬅️  Mover para ESQUERDA\n");
    printf("d. ➡️  Mover para DIREITA\n");
    printf("q. 🚪 Sair do jogo\n");
    printf("═══════════════════════════════\n");
}

int processar_comando_movimento(estado_cliente_t* cliente, char comando) {
    tipo_msg_t tipo_movimento;
    
    switch (comando) {
        case 'w':
            tipo_movimento = TIPO_DESLOCA_CIMA;
            break;
        case 's':
            tipo_movimento = TIPO_DESLOCA_BAIXO;
            break;
        case 'a':
            tipo_movimento = TIPO_DESLOCA_ESQUERDA;
            break;
        case 'd':
            tipo_movimento = TIPO_DESLOCA_DIREITA;
            break;
        default:
            printf("Comando inválido!\n");
            return -1;
    }
    
    printf("Enviando movimento: %s...\n", obter_nome_direcao(tipo_movimento));
    
    // Enviar movimento para o servidor
    if (enviar_movimento(cliente, tipo_movimento) < 0) {
        return -1;
    }
    
    // Processar resposta do servidor
    return processar_resposta_servidor(cliente);
}

int enviar_movimento(estado_cliente_t* cliente, tipo_msg_t tipo_movimento) {
    pack_t frame_movimento;
    cliente->protocolo.seq_atual = (cliente->protocolo.seq_atual + 1) % 32;
    
    criar_pack(&frame_movimento, cliente->protocolo.seq_atual, tipo_movimento, NULL, 0);
    
    return enviar_pack(&cliente->protocolo, &frame_movimento);
}

int processar_resposta_servidor(estado_cliente_t* cliente) {
    pack_t frame_resposta;
    frame_map_t frameMapa;

    // Receber resposta do servidor
    int result = receber_pack(&cliente->protocolo, &frame_resposta);
    if (result < 0) {
        if (result == -2) {
            printf("Timeout - servidor não respondeu\n");
        } else {
            printf("Erro ao receber resposta do servidor\n");
        }
        return -1;
    }

    uint8_t tipo = frame_resposta.tipo;
    uint8_t seq = getSeq(frame_resposta);
    int newTreasure = 0;
    switch (tipo) {
        case TIPO_ERRO:
            if (frame_resposta.dados[0] == ERRO_MOVIMENTO_INVALIDO) {
                printf("❌ Movimento inválido! Você não pode sair do mapa.\n");
            } else {
                printf("❌ Erro do servidor: %d\n", frame_resposta.dados[0]);
            }
            printf("Pressione ENTER para continuar...");
            getchar();
            return 0;

        case TIPO_MAPA_CLIENTE:

            memcpy(&frameMapa, frame_resposta.dados, sizeof(frame_map_t));
            attMap(&cliente->mapa_atual, frameMapa, &newTreasure);
            printf("✅ Movimento realizado!\n");

            if (cliente->mapa_atual.numTreasures > cliente->tesouros_coletados) {
                printf("🎉 Você achou um tesouro!\n");
                cliente->tesouros_coletados = cliente->mapa_atual.numTreasures;
            }
            enviar_ack(&cliente->protocolo, seq); // usa seq recuperado com macro
            if(frameMapa.findTreasure && newTreasure)
                receber_tesouro(cliente);
            return 0;
       
        default:
            printf("Resposta inesperada do servidor: tipo %d\n", tipo);
            return -1;
    }
}

int receber_tesouro(estado_cliente_t* cliente) {
    //Preparando pra receber o tipo tamanho
    pack_t pack;
    int debug = receber_pack(&cliente->protocolo, &pack);

    //tenta receber
    while(1){
        if (debug < 0) {
            printf("Erro ao receber informações do tesouro\n");
            debug = receber_pack(&cliente->protocolo, &pack);
            continue;
        }
        if(pack.tipo != TIPO_TAMANHO){
            debug = receber_pack(&cliente->protocolo, &pack);
            continue;
        }
        else{
            uint64_t tamanho_lido;
            enviar_ack(&cliente->protocolo, getSeq(pack));
            uint8_t tam[MAX_DADOS];
            memcpy(tam, pack.dados, sizeof(uint64_t));
            while(1){
                memcpy(&tamanho_lido, tam, sizeof(uint64_t));
                printf("Tamanho do arquivo = %llu bytes\n", (unsigned long long)tamanho_lido);
                break;
            }
            receber_pack(&cliente->protocolo, &pack);
            int tipo = pack.tipo;
            if(tipo >= TIPO_TEXTO_ACK_NOME && tipo <= TIPO_IMAGEM_ACK_NOME){
                enviar_ack(&cliente->protocolo, getSeq(pack));
                tipo_msg_t tipo_arquivo = pack.tipo;
                char nome_arquivo[256];

                strncpy(nome_arquivo, (char*)pack.dados, pack.tamanho);
                return receber_arquivo_tesouro(cliente, nome_arquivo, tipo_arquivo, tamanho_lido);
            }
            else
                continue;
            break;
        }
    }

    return 0;
}

int receber_arquivo_tesouro(estado_cliente_t* cliente, const char* nome_arquivo, tipo_msg_t tipo, uint64_t tamanho) {
    char caminho_completo[512];
    snprintf(caminho_completo, sizeof(caminho_completo), "%s%s", DIRETORIO_TESOUROS, nome_arquivo);

    FILE* arquivo = fopen(caminho_completo, "wb");
    if (!arquivo) {
        perror("Erro ao criar arquivo do tesouro");
        return -1;
    }

    uint64_t bytes_recebidos = 0;
    pack_t pack;

    while (bytes_recebidos < tamanho) {
        if (receber_pack(&cliente->protocolo, &pack) < 0) {
            printf("Erro ao receber dados do tesouro\n");
            fclose(arquivo);
            continue;
        }

        // Verificar tipo de dados
        if (pack.tipo != TIPO_DADOS){
            printf("Tipo de pacote inesperado: %d\n", pack.tipo);
            continue;
        }
        // Escrever dados no arquivo
        size_t bytes_escritos = fwrite(pack.dados, 1, pack.tamanho, arquivo);
      //  bytes_escritos+=1;
        
        if (bytes_escritos != pack.tamanho) {
            perror("Erro ao escrever no arquivo");
            fclose(arquivo);
            continue;
        }
        bytes_recebidos += bytes_escritos;
        enviar_ack(&cliente->protocolo, getSeq(pack));
    }
    fclose(arquivo);


    // Se estiver rodando como root, tenta pegar o dono real
    const char* user_name = getenv("SUDO_USER");
    if (!user_name) {
        // Não foi com sudo, usa o UID atual mesmo
        struct passwd* pw = getpwuid(getuid());
        if (pw) {
            chown(caminho_completo, pw->pw_uid, pw->pw_gid);
        }
    } else {
        // Foi com sudo, pega informações do usuário real
        struct passwd* pw = getpwnam(user_name);
        if (pw) {
            chown(caminho_completo, pw->pw_uid, pw->pw_gid);
        }
    }
    exibir_tesouro(nome_arquivo, caminho_completo, tipo);

    return 0;
}


void exibir_texto(const char *caminho_arquivo) {
    FILE *arquivo = fopen(caminho_arquivo, "r");
    if (arquivo == NULL) {
        perror("Erro ao abrir arquivo");
        return;
    }

    char linha[1024];
    printf("\n--- Conteúdo do Tesouro (Texto) ---\n");
    while (fgets(linha, sizeof(linha), arquivo)) {
        printf("%s", linha);
    }
    fclose(arquivo);

    printf("\n\nPressione ENTER para continuar...\n");
    getchar();

}

void exibir_imagem(const char *caminho) {
    // Tenta vários visualizadores possíveis
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "xdg-open %s || display %s || eog %s &", caminho, caminho, caminho);
    system(cmd);

    printf("Pressione ENTER para continuar...\n");
    getchar();
}

void exibir_video(const char *caminho) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "xdg-open \"%s\" > /dev/null 2>&1 || mpv \"%s\" --quiet > /dev/null 2>&1 || vlc \"%s\" --play-and-exit > /dev/null 2>&1 &",
        caminho, caminho, caminho);
    system(cmd);

    printf("Pressione ENTER para continuar...\n");
    getchar();
}





void exibir_tesouro(const char* nome_arquivo, const char* caminho_arquivo, tipo_msg_t tipo){
    switch(tipo){
        case TIPO_VIDEO_ACK_NOME:
            printf("🎥 Vídeo misterioso (%s)\n", nome_arquivo);
            exibir_video(caminho_arquivo);
            return;
        case TIPO_IMAGEM_ACK_NOME:
            printf("🖼️ Uma imagem rara (%s)\n", nome_arquivo);
            exibir_imagem(caminho_arquivo);
            return;
        case TIPO_TEXTO_ACK_NOME:
            printf("📄 Texto secreto (%s)\n", nome_arquivo);
            exibir_texto(caminho_arquivo);
            return;
        default:
            return;
    }
}






const char* obter_nome_direcao(tipo_msg_t tipo) {
    switch (tipo) {
        case TIPO_DESLOCA_CIMA: return "CIMA";
        case TIPO_DESLOCA_BAIXO: return "BAIXO";
        case TIPO_DESLOCA_ESQUERDA: return "ESQUERDA";
        case TIPO_DESLOCA_DIREITA: return "DIREITA";
        default: return "DESCONHECIDO";
    }
}
