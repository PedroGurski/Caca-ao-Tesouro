#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>

#include <termios.h>
#include <unistd.h>      // Para usleep()
#include <sys/time.h>    // Para struct timeval
#include <strings.h>     // Para strcasecmp()

// Constantes do protocolo
#define MAX_DADOS 127
#define MAX_TENTATIVAS 3
#define TIMEOUT_SEGUNDOS 10
#define TOLERANCIA_ESPACO 1048576  // 1MB
#define TAMANHO_MAX_NOME 63


// Constantes do jogo
#define TAMANHO_GRID 8
#define NUM_TESOUROS 8
#define DIRETORIO_OBJETOS "./objetos/"

// Tipos de mensagem
typedef enum {
    TIPO_ACK = 0,                   //ok
    TIPO_NACK = 1,                  //ok
    TIPO_OK_ACK = 2,                //ok
    TIPO_INICIAR_JOGO = 3,          //ok
    TIPO_TAMANHO = 4,               //ok
    TIPO_DADOS = 5,                 //ok
    TIPO_TEXTO_ACK_NOME = 6,        //ok
    TIPO_VIDEO_ACK_NOME = 7,        //ok
    TIPO_IMAGEM_ACK_NOME = 8,       //ok
    // Novos tipos para o jogo
    TIPO_FIM_ARQUIVO = 9,           //ok
    TIPO_DESLOCA_DIREITA = 10,      //ok
    TIPO_DESLOCA_CIMA = 11,         //ok
    TIPO_DESLOCA_BAIXO = 12,        //ok
    TIPO_DESLOCA_ESQUERDA = 13,     //ok
    TIPO_MAPA_CLIENTE = 14,         //ok
    TIPO_ERRO = 15,                 //ok
} tipo_msg_t;

// Tipos de erro
typedef enum {
    ERRO_ARQUIVO_NAO_ENCONTRADO = 1,
    ERRO_SEM_PERMISSAO = 2,
    ERRO_ESPACO_INSUFICIENTE = 3,
    ERRO_ARQUIVO_MUITO_GRANDE = 4,
    ERRO_MOVIMENTO_INVALIDO = 5
} tipo_erro_t;


#pragma pack(push, 1)  // Força alinhamento sem padding
typedef struct {
    // Byte 0
    uint8_t marcador;   // 8 bits (0x7E)
    
    // Byte 1
    uint8_t tamanho : 7; // 7 bits (tamanho)
    uint8_t seq_bit0 : 1; // 1 bit (LSB da sequência)
    
    // Byte 2
    uint8_t seq_bits1_4 : 4; // 4 bits (restante da sequência)
    uint8_t tipo : 4;        // 4 bits (tipo)
    
    // Byte 3
    uint8_t checksum;   // 8 bits (checksum)
    
    // Dados (127 bytes)
    uint8_t dados[127]; 
} pack_t;
#pragma pack(pop)


// Tipos de tesouro
typedef enum {
    TESOURO_TEXTO = 0,
    TESOURO_IMAGEM = 1,
    TESOURO_VIDEO = 2
} tipo_tesouro_t;

// Estrutura de posição
typedef struct {
    int x;
    int y;
} posicao_t;

// Estrutura de tesouro
typedef struct {
    posicao_t posicao;
    char nome_arquivo[64];
    char caminho_completo[256];
    uint8_t tamanho[MAX_DADOS];
    tipo_tesouro_t tipo;
    int encontrado;
} tesouro_t;

// Estado do protocolo
typedef struct {
    int socket_fd;
    struct sockaddr_in endereco_remoto;
    socklen_t tamanho_endereco;
    unsigned char seq_atual;
    unsigned char seq_esperada;
} estado_protocolo_t;

// Estado do jogo
typedef struct {
    posicao_t posicao_jogador;
    tesouro_t tesouros[NUM_TESOUROS];
    int grid_visitado[TAMANHO_GRID][TAMANHO_GRID];
    int tesouros_encontrados;
    int jogo_iniciado;
} estado_jogo_t;


#pragma pack(push, 1)
typedef struct{
    posicao_t playerPos;
    uint8_t findTreasure;
} frame_map_t;
#pragma pack(pop)

// Estrutura para informações do mapa do cliente
typedef struct {
    posicao_t playerPos;
    posicao_t colletTreasures[NUM_TESOUROS];
    char grid_visitado[TAMANHO_GRID][TAMANHO_GRID];
    int numTreasures;
} mapa_cliente_t;

// Estrutura para o estado do cliente
typedef struct {
    estado_protocolo_t protocolo;
    mapa_cliente_t mapa_atual;
    int jogo_ativo;
    int tesouros_coletados;
} estado_cliente_t;

// Funções do protocolo
int criar_pack(pack_t* pack, unsigned char seq, tipo_msg_t tipo, 
               uint8_t* dados, unsigned short tamanho);
int enviar_pack(estado_protocolo_t* estado, const pack_t* pack);
int receber_pack(estado_protocolo_t* estado, pack_t* pack);
int enviar_ack(estado_protocolo_t* estado, uint8_t seq);
int enviar_nack(estado_protocolo_t* estado, uint8_t seq);
int enviar_ok_ack(estado_protocolo_t* estado, uint8_t seq);
int enviar_erro(estado_protocolo_t* estado, uint8_t seq, tipo_erro_t erro);
int esperar_ack(estado_protocolo_t* estado);

// Funções do jogo
void inicializar_jogo(estado_jogo_t* jogo);
int mover_jogador(estado_jogo_t* jogo, tipo_msg_t direcao);
int verificar_tesouro(estado_jogo_t* jogo, posicao_t posicao);
void mostrar_mapa_servidor(estado_jogo_t* jogo);
void mostrar_mapa_cliente(estado_cliente_t* cliente);
void limpar_tela();

// Funções auxiliares
uint8_t getSeq(pack_t pack);
void obter_tamanho_arquivo(const char* caminho, uint8_t tam[MAX_DADOS]);
unsigned long obter_espaco_livre(const char* diretorio);
int is_arquivo_regular(const char* caminho);
tipo_msg_t determinar_tipo_arquivo(const char* nome_arquivo);

#endif // PROTOCOLO_H
