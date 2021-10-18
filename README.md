# TCP IP lab
Tarefa do laboratório consiste em implementação de um sistema servidor/cliente
TCP para troca de mensagens entre processos, onde o cliente enviará requisições
de execução de comandos de shell script no servidor e receber em resposta o
resultado dessa consulta.
A condição de finalização do servidor é o envio de um comando "quit" por parte
do cliente para o servidor.
Outra condição de contorno necessária desse trabalho é a implementação de um
contador de requisições, em que cada request de cada cliente será incrementado
e enviado junto à resposta.

---

## Passo-a-passo
 - [X] Implementação do servidor
 - [X] Implementação do cliente
 - [X] Teste do comando de quit
 - [ ] Teste de comandos shell genéricos
    - Esse comando apresentou problemas: pela implementação simples usando a
    chamada popen() para executar um outro processo, alguns tipos de chamadas do
    cliente para o servidor resultam num "hang". Ainda preciso depurar o funcio-
    -namento em casos como esse; acredito que usando fork(), execv() e pipes não
    aconteceria esse tipo de problema.
 - [X] Implementação da variável contadora de número de atendimento