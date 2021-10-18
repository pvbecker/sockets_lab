#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../common/message.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>

// Shared memory for parent and child processes for keeping track of service count
#define SHARED_MEMORY_NAME    "/server-shared-memory"
#define SHARED_MEM_PERMISSIONS  0660

// Semaphores for critical regions: updating service count and number of connections.
#define SERVICE_COUNT_SEMAPHORE_NAME "/posix-service-count-semaphore"
#define CONNECTION_COUNT_SEMAPHORE_NAME "/posix-connection-count-semaphore"
#define SEMAPHORE_PERMISSIONS 0660

#define CRITICAL_SECTION_INCREMENT(sem, X) {    \
    sem_wait(sem);                              \
    X++;                                        \
    sem_post(sem);                              \
}                                               \

#define CRITICAL_SECTION_DECREMENT(sem, X) {    \
    sem_wait(sem);                              \
    X--;                                        \
    sem_post(sem);                              \
}                                               \

char status_msg[] =
"+--------------------------------------\n\
| number of active connections | %d    \n\
+--------------------------------------\n\
| service count                | %d    \n\
+--------------------------------------\n";

int shared_mem_id; //shared memory id for number of connections counter
struct shared_memory {
    int num_active_connections;
    int service_count;
};
struct shared_memory *pshared_mem;


void inspect_variable(char *message, uint16_t variable){
    uint8_t *aux = malloc(sizeof(uint8_t) * sizeof(uint16_t));
    memcpy(aux, &variable, sizeof(uint8_t) * sizeof(uint16_t));
    printf("%s", message);
    for(int i = 0; i < 2; i++){
        printf("0x%02x ", aux[i]);
    }
    free(aux);
    printf("\n");
}

typedef struct {
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    int server_socket;              // "file descriptor" for server socket
    int client_socket;              // "file descriptor" for client socket
    int client_name_len;            // size of client's socket structure.
    sem_t *service_count_sem;
    sem_t *connection_count_sem;
} context_t;


void print_status(){
    system("clear");
    printf(status_msg, pshared_mem->num_active_connections, pshared_mem->service_count);
}

void initialize_shared_memory(void){
    if ((shared_mem_id = shm_open(SHARED_MEMORY_NAME, O_RDWR | O_CREAT, SHARED_MEM_PERMISSIONS)) == -1) {
        perror("shm_open");
        exit(1);
    }

    if (ftruncate(shared_mem_id, sizeof(struct shared_memory)) == -1) {
        perror("ftruncate");
        exit(1);
    }

    if ((pshared_mem = mmap(NULL, sizeof(struct shared_memory), PROT_READ | PROT_WRITE, MAP_SHARED, shared_mem_id, 0)) == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    pshared_mem->num_active_connections = 0;
    pshared_mem->service_count = 0;
}

void initialize_semaphores(context_t *context){
    context->connection_count_sem = sem_open(CONNECTION_COUNT_SEMAPHORE_NAME, O_CREAT, SEMAPHORE_PERMISSIONS, 1);
    if(context->connection_count_sem == SEM_FAILED){
        perror("error on connection count semaphore creation: ");
        exit(-2);
    }

    context->service_count_sem = sem_open(SERVICE_COUNT_SEMAPHORE_NAME, O_CREAT, SEMAPHORE_PERMISSIONS, 1);
    if(context->service_count_sem == SEM_FAILED){
        perror("error on service count semaphore creation: ");
        exit(-2);
    }
}

int serve_child(context_t *context){
    printf("serving child process...\n");
    // closes this process for incoming connections.
    close(context->server_socket);
    do{
        message_t in_msg;
        // get incoming message
        if(recv(context->client_socket, (void *)&in_msg, sizeof(in_msg), 0) == -1){
            perror("error on recv ");
            exit(6);
        }
        // removes the last \n from the input command
        int length = strlen(in_msg.cmd);
        in_msg.cmd[length - 1] = '\0';
        if(!memcmp(in_msg.cmd, "quit", strlen("quit"))){
            printf("Bye\n");
            break;
        }
        // this approach, although easy to implement, is giving me trouble when trying to run more
        // complex commands and not getting the stderr output redirected to the file. This approach
        // is only redirecting stdout to the nameless pipe.
        char *ptr = in_msg.cmd;
        FILE *fd = popen(in_msg.cmd, "r");
        if(fd == NULL){ //popen failed
            printf("failed on popen\n");
            return -1;  //TODO: create return codes enumerations.
        }
        // succeeded on popen, read from fd (child process' output to stdout) into a rsp_msg command
        // buffer, which will always be initialized with all zeroes.
        message_t rsp_msg = {0};
        rsp_msg.service_count = pshared_mem->service_count;
        int i = 0;
        while(!feof(fd)){
            fread(&rsp_msg.cmd[i++], 1, 1, fd);
        }
        // critial section for service count
        CRITICAL_SECTION_INCREMENT(context->service_count_sem, pshared_mem->service_count);
        int payload = strlen(rsp_msg.cmd) + sizeof(rsp_msg.service_count);
        int sent = 0;
        while(sent < payload){
            int aux = 0;
            aux = send(context->client_socket, ((void *)&rsp_msg) + sent, payload - sent, 0);
            if(aux < 0){
                perror("error on send(): ");
                return 7;
            }
            sent += aux;
        }
        print_status();
    } while(true);
    // sends message to client in order to finish execution
    message_t rsp_msg = {0};
    strcpy(rsp_msg.cmd, KILL_CODE);
    send(context->client_socket, ((void *)&rsp_msg), sizeof(rsp_msg), 0);

    close(context->client_socket);

    // critial section for active connections
    CRITICAL_SECTION_DECREMENT(context->connection_count_sem, pshared_mem->num_active_connections);

    print_status();

    return 0;
}

int main(int argc, char **argv){
    int result = 0;
    pid_t pid;
    uint16_t port;
    context_t context = {0};
    // check for input arguments. Needs a port number for the application
    if(argc < 2){
        printf("Usage: server <port number>\n");
        return 1;
    }

    initialize_shared_memory();
    initialize_semaphores(&context);

    // get port number
    port = (uint16_t) atoi(argv[1]);
    // since port numbers are expected to be set as big-endian, one must convert the number read
    // from the program's command line arguemnts from local machine's endianess to big-endian.
    uint16_t value = htons(port);

    // create socket
    context.server_socket = socket(PF_INET, SOCK_STREAM, 0);
    if(context.server_socket < 0){
        perror("error on socket(): ");
        return 2;
    }

    // defines server-side socket protocol family and defines appropriate values for each member of
    // the address struct.
    context.server_addr.sin_family = AF_INET;
    context.server_addr.sin_port = htons(port);
    context.server_addr.sin_addr.s_addr = INADDR_ANY;

    // connect server to previously assigned port
    result = bind(context.server_socket,
                  (struct sockaddr *)&context.server_addr,
                  sizeof(context.server_addr));
    if(result < 0){
        perror("error on bind(): ");
        return 3;
    }

    // listen for incomming connections on recently-created socket. Up to this point, if we inspect
    // the socket states with netstat command, no socket has been created. If this call successfully
    // executes, a port will be created and netstat will list the port. Since this server-side
    // socket was created with INADDR_ANY, the listing of IP address will show up as 0.0.0.0:<port>.
    // In order to see all this, one can run
    // watch -n<interval in seconds> netstat -antpo
    result = listen(context.server_socket, 1);
    if(result != 0){
        perror("error on listen() ");
        return 4;
    }

    // main loop
    while(1){
        // printf("waiting for new connection...\n");
        print_status();
        // accepts incomming connection
        int new_sock;
        context.client_name_len = sizeof(context.client_addr);
        context.client_socket = accept(context.server_socket,
                                       (struct sockaddr *)&context.client_addr,
                                       (socklen_t *)&context.client_name_len);
        if(context.client_socket == -1){
            perror("error on accept()");
            return 5;
        }
        // up to this point, the connection has not yet persisted when analyzing the status of
        // connections using netstat.

        // since we'll be dealing with a bunch of requests coming in the form of shell commands, we
        // can simply use the fork/execvp syscalls in order to serve the request, get the result
        // from the child process using the wait() syscall and responding to the client with the
        // outcome of the command's execution on the server-side.
        if( (pid = fork()) == 0){
            //child process
            pid_t child_pid = getpid();
            serve_child(&context);
            exit(0);
        }
        else{
            // parent process
            // critical section for connection count
            CRITICAL_SECTION_INCREMENT(context.connection_count_sem, pshared_mem->num_active_connections);
            printf("waiting on child process\n");
            close(context.client_socket);
        }
    }
    printf("done\n");

    return 0;
}