#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/message.h"

#define VERSION "0.0.0"
#define MAX_NUM_ARGS
#define MAX_CMD_LEN 250

void print_prompt(int count){
    count > 0 ? printf("(-)$: ") : printf("(%d)$: ", count);
}

int main(int argc, char **argv){

    int result;
    unsigned short port;
    message_t sendBuffer;
    message_t receiveBuffer;
    struct hostent *hostname;
    struct sockaddr_in server;
    int sckt;

    if(argc < 3){
        printf("Usage: %s <host> <port>", argv[0]);
        exit(1);
    }

    hostname = gethostbyname(argv[1]);
    if(hostname == (struct hostent *)0){
        perror("hostname failed ");
        exit(2);
    }
    port = (unsigned short) atoi(argv[2]);

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = *((unsigned long *)hostname->h_addr_list[0]);

    sckt = socket(PF_INET, SOCK_STREAM, 0);
    if(sckt < 0){
        perror("error on creating socket: ");
        exit(3);
    }

    result = connect(sckt, (struct sockaddr *)&server, sizeof(server));
    if(result < 0){
        perror("error on connect(): ");
        exit(4);
    }

    // needs to create the message to be sent. in this case, we need to start asking for user input
    // for command line entries to be sent over via thetcp socket.
    printf("client tcp application, version %s\n\n", VERSION);
    do{
        message_t out_msg = {0};
        message_t rsp_msg = {0};
        rsp_msg.service_count = -1;
        printf("$: ");
        fgets(out_msg.cmd, MAX_CMD_LEN, stdin);
        // now, we need to establish a connection and actually send data to the server side.
        if(send(sckt, (void *)&out_msg, sizeof(out_msg), 0) < 0){
            perror("error on send: ");
            exit(5);
        }
        // receive message from server
        if(recv(sckt, (void *)&rsp_msg, sizeof(rsp_msg), 0) == -1){
            perror("error on recv ");
            exit(6);
        }
        printf("service count: %d\n", rsp_msg.service_count);
        printf("%s", rsp_msg.cmd);
        if(!memcmp(rsp_msg.cmd, KILL_CODE, KILL_CODE_LEN)){
            printf("Done. Bye.\n");
            break;
        }
    }while(1);
}