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

# Arquivos objeto
PROTOCOL_OBJ = protocolo.o
SERVIDOR_OBJ = servidor.o
CLIENTE_OBJ = cliente.o

# Arquivos de cabeçalho
HEADERS = protocol.h

# Diretórios
ARQUIVOS_DIR = arquivos
DOWNLOADS_DIR = downloads

# Regra padrão
all: $(SERVIDOR) $(CLIENTE) setup

# Compilar servidor
$(SERVIDOR): $(SERVIDOR_OBJ) $(PROTOCOL_OBJ)
	@echo "Linkando servidor..."
	$(CC) $(SERVIDOR_OBJ) $(PROTOCOL_OBJ) -o $(SERVIDOR) $(LDFLAGS)
	@echo "Servidor compilado com sucesso!"

# Compilar cliente
$(CLIENTE): $(CLIENTE_OBJ) $(PROTOCOL_OBJ)
	@echo "Linkando cliente..."
	$(CC) $(CLIENTE_OBJ) $(PROTOCOL_OBJ) -o $(CLIENTE) $(LDFLAGS)
	@echo "Cliente compilado com sucesso!"

# Compilar arquivos objeto
%.o: %.c $(HEADERS)
	@echo "Compilando $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Criar diretórios e arquivos de teste
setup: $(ARQUIVOS_DIR) $(DOWNLOADS_DIR) test-files
	@echo "Ambiente configurado com sucesso!"

# Criar diretório de arquivos do servidor
$(ARQUIVOS_DIR):
	@echo "Criando diretório $(ARQUIVOS_DIR)..."
	mkdir -p $(ARQUIVOS_DIR)

# Criar diretório de downloads do cliente
$(DOWNLOADS_DIR):
	@echo "Criando diretório $(DOWNLOADS_DIR)..."
	mkdir -p $(DOWNLOADS_DIR)

# Criar arquivos de teste
test-files: $(ARQUIVOS_DIR)
	@echo "Criando arquivos de teste..."
	@if [ ! -f $(ARQUIVOS_DIR)/documento.txt ]; then \
		echo "Este é um arquivo de texto de exemplo para teste do protocolo." > $(ARQUIVOS_DIR)/documento.txt; \
		echo "Linha 2 do documento de teste." >> $(ARQUIVOS_DIR)/documento.txt; \
		echo "Fim do arquivo de teste." >> $(ARQUIVOS_DIR)/documento.txt; \
		echo "Criado: $(ARQUIVOS_DIR)/documento.txt"; \
	fi
	@if [ ! -f $(ARQUIVOS_DIR)/imagem.jpg ]; then \
		echo "Aviso: Coloque uma imagem real em $(ARQUIVOS_DIR)/imagem.jpg para teste completo"; \
		touch $(ARQUIVOS_DIR)/imagem.jpg; \
	fi
	@if [ ! -f $(ARQUIVOS_DIR)/musica.mp3 ]; then \
		echo "Aviso: Coloque um arquivo MP3 real em $(ARQUIVOS_DIR)/musica.mp3 para teste completo"; \
		touch $(ARQUIVOS_DIR)/musica.mp3; \
	fi

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
	@echo "Iniciando cliente (conectando em localhost)..."
	./$(CLIENTE) 127.0.0.1

# Executar cliente (IP customizado)
run-cliente-ip: $(CLIENTE) setup
	@echo "Para conectar em um IP específico, use: make run-cliente-custom IP=<endereço>"
	@echo "Exemplo: make run-cliente-custom IP=192.168.1.100"

run-cliente-custom: $(CLIENTE) setup
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
	rm  downloads/*
	@echo "Limpeza concluída!"

# Limpeza completa (incluindo diretórios e arquivos de teste)
clean-all: clean
	@echo "Removendo diretórios de teste..."
	rm -rf $(ARQUIVOS_DIR) $(DOWNLOADS_DIR)
	@echo "Limpeza completa concluída!"

# Instalar dependências (Ubuntu/Debian)
install-deps:
	@echo "Instalando dependências necessárias..."
	sudo apt-get update
	sudo apt-get install -y gcc make libc6-dev

# Debug - compilação com símbolos de debug
debug: CFLAGS += -DDEBUG -O0
debug: clean all
	@echo "Versão debug compilada!"

# Release - compilação otimizada
release: CFLAGS += -O2 -DNDEBUG
release: clean all
	@echo "Versão release compilada!"

# Verificar se há vazamentos de memória (requer valgrind)
valgrind-servidor: $(SERVIDOR) setup
	valgrind --leak-check=full --show-leak-kinds=all ./$(SERVIDOR)

valgrind-cliente: $(CLIENTE) setup
	valgrind --leak-check=full --show-leak-kinds=all ./$(CLIENTE) 127.0.0.1

# Executar testes básicos
test: $(SERVIDOR) $(CLIENTE) setup
	@echo "=== Executando testes básicos ==="
	@echo "1. Verificando se executáveis foram criados..."
	@test -x $(SERVIDOR) && echo "✓ Servidor executável" || echo "✗ Servidor não encontrado"
	@test -x $(CLIENTE) && echo "✓ Cliente executável" || echo "✗ Cliente não encontrado"
	@echo "2. Verificando estrutura de diretórios..."
	@test -d $(ARQUIVOS_DIR) && echo "✓ Diretório $(ARQUIVOS_DIR) existe" || echo "✗ $(ARQUIVOS_DIR) não encontrado"
	@test -d $(DOWNLOADS_DIR) && echo "✓ Diretório $(DOWNLOADS_DIR) existe" || echo "✗ $(DOWNLOADS_DIR) não encontrado"
	@echo "3. Verificando arquivos de teste..."
	@test -f $(ARQUIVOS_DIR)/documento.txt && echo "✓ Arquivo de teste criado" || echo "✗ Arquivo de teste não encontrado"
	@echo "=== Testes básicos concluídos ==="

# Mostrar informações sobre o projeto
info:
	@echo "=== PROTOCOLO CLIENTE-SERVIDOR ==="
	@echo "Arquivos fonte:"
	@echo "  - $(PROTOCOL_SRC) (implementação do protocolo)"
	@echo "  - $(SERVIDOR_SRC) (servidor de arquivos)"
	@echo "  - $(CLIENTE_SRC) (cliente receptor)"
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

# Verificar se o nome do header está correto
check-header:
	@echo "Verificando consistência dos headers..."
	@if [ -f "protoco.h" ] && [ ! -f "protocol.h" ]; then \
		echo "⚠️  Encontrado 'protoco.h' mas esperado 'protocol.h'"; \
		echo "   Renomeando automaticamente..."; \
		mv protoco.h protocol.h; \
		echo "✓ Header renomeado para protocol.h"; \
	elif [ -f "protocol.h" ]; then \
		echo "✓ Header protocol.h encontrado"; \
	else \
		echo "✗ Nenhum header encontrado (protoco.h ou protocol.h)"; \
	fi

# Phony targets (não são arquivos)
.PHONY: all clean clean-all setup test-files run-servidor run-cliente run-cliente-ip run-cliente-custom
.PHONY: debug release valgrind-servidor valgrind-cliente test info help install-deps check-header
.PHONY: servidor cliente
