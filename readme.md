
# Projeto: Módulo de Kernel para Fila de Mensagens Entre Processos

Projeto desenvolvido como requisito parcial de avaliação **(T2)** da disciplina de **Construção de Sistemas Operacionais** do curso de Engenharia de Computação, na **PUCRS**.

## Alunos Desenvolvedores
- **Jhonathan Araújo Mesquita de Freitas**
- **Tomás Haddad Caldas**
- **William Matheus de Oliveira Rodrigues**

## Professor Orientador
- **Prof. Dr. Sergio Johann Filho**



## Sumário
- [Descrição Geral](#descrição-geral)
- [Ambiente de Desenvolvimento](#ambiente-de-desenvolvimento)
- [Componentes do Projeto](#componentes-do-projeto)
  - [t2_driver.c](#t2_driverc)
  - [test_driver.c](#test_driverc)
  - [Makefile](#makefile)
- [Instruções de Compilação e Execução](#instruções-de-compilação-e-execução)
  - [Compilação do Módulo e Aplicação](#compilação-do-módulo-e-aplicação)
  - [Preparação do Ambiente](#preparação-do-ambiente)
  - [Execução no QEMU](#execução-no-qemu)
- [Conclusão](#conclusão)
- [Contato](#contato)

## Descrição Geral
O objetivo deste projeto é desenvolver um módulo de kernel carregável (**KLM - Kernel Loadable Module**) para o sistema operacional Linux. O módulo implementa um mecanismo de distribuição de mensagens entre processos utilizando um sistema de filas circulares, acessível através de `/dev/t2_driver`.

### Principais funcionalidades:
- **Registro de Processos**: Processos podem se registrar no módulo, associando um nome ao seu PID.
- **Envio de Mensagens**: Processos registrados podem enviar mensagens para outros processos registrados utilizando seus nomes.
- **Recebimento de Mensagens**: Processos podem ler mensagens endereçadas a eles, uma por vez, em ordem de chegada.
- **Desregistro de Processos**: Processos podem se desregistrar, e suas filas de mensagens são descartadas.

O sistema de filas circulares permite o armazenamento das últimas mensagens enviadas a um determinado processo, com capacidade limitada para evitar consumo excessivo de memória.

## Ambiente de Desenvolvimento
- **Sistema Operacional**: Linux
- **Versão do Kernel**: 4.13.9
- **Compilador**: GCC
- **Ferramentas Utilizadas**:
  - **Buildroot**: Para geração da imagem do sistema.
  - **QEMU**: Para emulação do ambiente.
  - **Diretório do Kernel**: `/home/labredes/linuxdistro/linux-4.13.9/`

## Componentes do Projeto

### t2_driver.c
Este é o módulo de kernel que implementa o driver de fila de mensagens entre processos.

#### Principais funcionalidades:
- **Parâmetros do Módulo**: Aceita parâmetros para definir o número máximo de mensagens por processo (`max_messages`) e o tamanho máximo de cada mensagem (`max_message_size`).
- **Gestão de Processos e Filas**: Gerencia uma lista de processos registrados e suas respectivas filas de mensagens, utilizando listas ligadas do kernel.
- **Operações de Leitura e Escrita**:
  - **Leitura (dev_read)**: Processos leem mensagens destinadas a eles.
  - **Escrita (dev_write)**: Processos podem registrar-se, desregistrar-se ou enviar mensagens para outros processos.
- **Tratamento de Erros**: Emite alertas no log do kernel em condições de erro, como:
  - Envio de mensagens para processos não registrados.
  - Mensagens que excedem os limites estabelecidos.
  - Exceder o número máximo de mensagens na fila (mensagens mais antigas são descartadas).

##### Exemplo de Uso dos Parâmetros:
```bash
modprobe t2_driver max_messages=5 max_message_size=250
```

### test_driver.c
Aplicação em espaço de usuário que serve para validar o funcionamento do módulo de kernel.

#### Principais funcionalidades:
- **Registro e Desregistro**: Permite que o usuário registre e desregistre processos no driver, associando um nome ao processo.
- **Envio de Mensagens**: Envia mensagens para outros processos registrados, utilizando seus nomes.
- **Leitura de Mensagens**: Lê mensagens endereçadas ao processo que está executando a aplicação.
- **Interface Interativa**: Fornece um menu interativo para facilitar a interação com o módulo.

##### Exemplo de Interação:
```bash
Choose an action:
1. Send a message
2. Read messages
3. Unregister and exit
Enter your choice:
```

### Makefile
Arquivo de automação de compilação que facilita o processo de build do projeto.

#### Principais funcionalidades:
- **Compilação do Módulo de Kernel (t2_driver.c)**: Utiliza as ferramentas do kernel para compilar o módulo.
- **Compilação da Aplicação de Teste (test_driver.c)**: Compila a aplicação de usuário com o GCC.
- **Recompilação da Imagem do Sistema**: Recompila a imagem do sistema para incluir o módulo recém-compilado.


## Instruções de Compilação e Execução

### Compilação do Módulo e Aplicação
1. **Exportar Diretório do Kernel**:
   Antes de compilar, é necessário exportar a variável de ambiente que aponta para o diretório do código-fonte do kernel:
   ```bash
   export LINUX_OVERRIDE_SRCDIR=/home/labredes/linuxdistro/linux-4.13.9/
   ```

2. **Compilar o Módulo e a Aplicação**:
   Utilize o comando `make`, (estando na pasta ../buildroot*/modules/t2_driver) para compilar o módulo de kernel e a aplicação de teste:
   ```bash
   make
   ```

Isso irá gerar os arquivos:
- `t2_driver.ko` (módulo do kernel)
- `test_driver` (aplicação de usuário, que será copiada para /output/target/bin)



### Preparação do Ambiente
1. **Recompilar a Imagem do Sistema**:
   Após compilar o módulo, é necessário recompilar a imagem do sistema para incluir o módulo recém-criado (estando na pasta ../buildroot*/):
   ```bash
   make
   ```

2. **Iniciar o Sistema no QEMU**:
   Utilize o seguinte comando para iniciar o sistema emulado no QEMU:
   ```bash
   qemu-system-i386 --kernel output/images/bzImage --hda output/images/rootfs.ext2 --nographic --append "console=ttyS0 root=/dev/sda"
   ```

### Execução no QEMU
1. **Carregar o Módulo de Kernel**:
   Dentro do ambiente emulado, carregue o módulo com os parâmetros desejados:
   ```bash
   modprobe t2_driver max_messages=5 max_message_size=250
   ```

2. **Executar a Aplicação de Teste**:
   Em diferentes terminais (ou processos em background), execute a aplicação de teste:
   ```bash
   ./test_driver
   ```

3. **Interagindo com o Módulo**:
   - **Registrar Processos**:
     Escolha a opção de registrar e forneça um nome único para o processo:
     ```bash
     Enter a name to register with the message queue: processo1
     Registered as 'processo1'
     ```

   - **Enviar Mensagens**:
     Envie mensagens para outros processos registrados:
     ```bash
     Enter the destination process name: processo2
     Enter the message to send: Olá, processo2!
     Message sent to 'processo2'
     ```

   - **Ler Mensagens**:
     Leia as mensagens endereçadas ao seu processo:
     ```bash
     Received message: 'Olá, processo2!'
     ```

   - **Desregistrar Processos**:
     Quando terminar, desregistre o processo:
     ```bash
     Unregistered successfully.
     ```

> **Nota**: Utilize o `dmesg` ou `tail -f /var/log/kern.log` para monitorar mensagens do kernel e depurar possíveis problemas.


## Conclusão
Este projeto implementa com sucesso um mecanismo de comunicação entre processos no espaço de usuário através de um módulo de kernel personalizado. A utilização de filas circulares e o gerenciamento eficiente de recursos demonstram a aplicação prática de conceitos avançados de sistemas operacionais e programação em kernel Linux.

## Contato
Para quaisquer dúvidas ou sugestões, entre em contato com os desenvolvedores:
- **Jhonathan Araújo Mesquita de Freitas** - [jhonathan.freitas@edu.pucrs.br](mailto:jhonathan.freitas@edu.pucrs.br)
- **Tomás Haddad Caldas** - [tomas.caldas@edu.pucrs.br](mailto:tomas.caldas@edu.pucrs.br)
- **William Matheus de Oliveira Rodrigues** - [william.rodrigues@edu.pucrs.br](mailto:william.rodrigues@edu.pucrs.br)

Ou com o professor orientador:
- **Prof. Dr. Sergio Johann Filho** - [sergio.filho@pucrs.br](mailto:sergio.filho@pucrs.br)
