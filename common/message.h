#define KILL_CODE       "finish connection\n"
#define KILL_CODE_LEN   18
#define MAX_MSG_LEN     32768

typedef struct message{
    int service_count;
    char cmd[MAX_MSG_LEN];
} message_t;