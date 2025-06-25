# Makefile para Protocolo Cliente-Servidor
# Compilador e flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pedantic -g
LDFLAGS = 

# Nomes dos executáveis
SERVIDOR = servidor
CLIENTE = cliente

# Arquivos fonte
PROTOCOL_SRC = protocolo.c
SERVIDOR_SRC = servidor.c
CLIENTE_SRC = cliente.c
RAWSOCKET_SRC = rawSocket.c

# Arquivos objeto
PROTOCOL_OBJ = protocolo.o
SERVIDOR_OBJ = servidor.o
CLIENTE_OBJ = cliente.o
RAWSOCKET_OBJ = rawSocket.o

# Arquivos de cabeçalho
HEADERS = protocol.h rawSocket.h

# Diretórios
ARQUIVOS_DIR = objetos
DOWNLOADS_DIR = downloads

# Regra padrão
all: $(SERVIDOR) $(CLIENTE) setup

# Compilar servidor
$(SERVIDOR): $(SERVIDOR_OBJ) $(PROTOCOL_OBJ) $(RAWSOCKET_OBJ)
	@echo "Linkando servidor..."
	$(CC) $(SERVIDOR_OBJ) $(PROTOCOL_OBJ) $(RAWSOCKET_OBJ) -o $(SERVIDOR) $(LDFLAGS)
	@echo "Servidor compilado com sucesso!"

# Compilar cliente
$(CLIENTE): $(CLIENTE_OBJ) $(PROTOCOL_OBJ) $(RAWSOCKET_OBJ)
	@echo "Linkando cliente..."
	$(CC) $(CLIENTE_OBJ) $(PROTOCOL_OBJ) $(RAWSOCKET_OBJ) -o $(CLIENTE) $(LDFLAGS)
	@echo "Cliente compilado com sucesso!"

# Compilar arquivos objeto
%.o: %.c $(HEADERS)
	@echo "Compilando $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Criar diretórios e arquivos de teste
setup: $(ARQUIVOS_DIR) $(DOWNLOADS_DIR) test
	@echo "Ambiente configurado com sucesso!"

# Verifica se diretório 'arquivos' existe, senão exibe erro e para o make
check-arquivos:
	@if [ ! -d $(ARQUIVOS_DIR) ]; then \
		echo "Erro: Diretório '$(ARQUIVOS_DIR)' não encontrado!"; \
		exit 1; \
	else \
		echo "Diretório '$(ARQUIVOS_DIR)' encontrado."; \
	fi

# Criar diretório de downloads do cliente (apenas se não existir)
check-downloads:
	@if [ ! -d $(DOWNLOADS_DIR) ]; then \
		echo "Criando diretório $(DOWNLOADS_DIR)..."; \
		mkdir -p $(DOWNLOADS_DIR); \
	else \
		echo "Diretório $(DOWNLOADS_DIR) já existe."; \
	fi

# Verifica se os arquivos de teste existem dentro do diretório 'arquivos'
test: check-arquivos
	@echo "Verificando arquivos de teste em $(ARQUIVOS_DIR)..."
	@for file in 1.txt 2.txt 3.txt 4.jpg 5.jpg 6.jpg 7.mp4 8.mp4; do \
		if [ -f $(ARQUIVOS_DIR)/$$file ]; then \
			echo "✓ Encontrado: $(ARQUIVOS_DIR)/$$file"; \
		else \
			echo "✗ Faltando: $(ARQUIVOS_DIR)/$$file"; \
		fi \
	done

# Compilar apenas o servidor
servidor: $(SERVIDOR)

# Compilar apenas o cliente
cliente: $(CLIENTE)

# Executar servidor
run-servidor: $(SERVIDOR) setup
	@echo "Iniciando servidor..."
	./$(SERVIDOR)

# Executar cliente (localhost)
run-cliente: $(CLIENTE) setup
	@if [ -z "$(IP)" ]; then \
		echo "Erro: Especifique o IP com IP=<endereço>"; \
		echo "Exemplo: make run-cliente-custom IP=192.168.1.100"; \
		exit 1; \
	fi
	@echo "Iniciando cliente (conectando em $(IP))..."
	./$(CLIENTE) $(IP)

# Limpeza
clean:
	@echo "Removendo arquivos objeto e executáveis..."
	rm -f *.o $(SERVIDOR) $(CLIENTE)
	rm -f downloads/*
	@echo "Limpeza concluída!"

# Limpeza completa (incluindo diretórios e arquivos de teste)
clean-all: clean
	@echo "Removendo diretórios de trofeu..."
	rm -rf $(DOWNLOADS_DIR)
	@echo "Limpeza completa concluída!"

# Mostrar informações sobre o projeto
info:
	@echo "=== PROTOCOLO CLIENTE-SERVIDOR ==="
	@echo "Arquivos fonte:"
	@echo "  - $(PROTOCOL_SRC) (implementação do protocolo)"
	@echo "  - $(SERVIDOR_SRC) (servidor de arquivos)"
	@echo "  - $(CLIENTE_SRC) (cliente receptor)"
	@echo "  - $(RAWSOCKET_SRC) (raw socket implementation)"
	@echo ""
	@echo "Executáveis:"
	@echo "  - $(SERVIDOR) (servidor)"
	@echo "  - $(CLIENTE) (cliente)"
	@echo ""
	@echo "Diretórios:"
	@echo "  - $(ARQUIVOS_DIR)/ (arquivos do servidor)"
	@echo "  - $(DOWNLOADS_DIR)/ (downloads do cliente)"
	@echo ""
	@echo "Comandos úteis:"
	@echo "  make all          - Compilar tudo"
	@echo "  make servidor     - Compilar apenas servidor"
	@echo "  make cliente      - Compilar apenas cliente"
	@echo "  make run-servidor - Executar servidor"
	@echo "  make run-cliente  - Executar cliente (localhost)"
	@echo "  make test         - Executar testes básicos"
	@echo "  make clean        - Limpar arquivos compilados"
	@echo "  make clean-all    - Limpeza completa"

# Mostrar ajuda
help: info