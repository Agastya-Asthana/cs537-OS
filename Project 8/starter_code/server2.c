#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <pthread.h> 
#include "server_functions.h" 
#include "client.h" 
#include "udp.h" 

#define MAX_CLIENTS 100 // Call table entry 

struct call_entry {
    int client_id; 
    int seq_number; 
    int last_result; 
    int in_progress; 
}; // Global variables 
struct call_entry call_table[MAX_CLIENTS];
pthread_mutex_t call_table_lock; 
// Find or create call entry in the call table 
struct call_entry* find_call_entry(int client_id) {
    pthread_mutex_lock(&call_table_lock); 
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (call_table[i].client_id == client_id) {
            pthread_mutex_unlock(&call_table_lock);
            return &call_table[i]; 
        } else if (call_table[i].client_id == 0) { 
            call_table[i].client_id = client_id; 
            call_table[i].seq_number = 0; 
            call_table[i].last_result = -1; 
            call_table[i].in_progress = 0; 
            pthread_mutex_unlock(&call_table_lock); 
            return &call_table[i]; 
            } 
    } 
    pthread_mutex_unlock(&call_table_lock); 
    return NULL; 
} // Request handler argument structure 
struct request_handler_args { 
    struct rpc_connection rpc; 
    char request[BUFLEN]; 
}; // Request handler function 
void* request_handler(void* args) { 
    struct request_handler_args* req_args = (struct request_handler_args*)args; 
    struct rpc_connection rpc = req_args->rpc; 
    char* request = req_args->request; 
    int client_id, seq_number, arg1, arg2; 
    char cmd[10]; sscanf(request, "%9[^,],%d,%d,%d,%d", cmd, &client_id, &seq_number, &arg1, &arg2); 
    struct call_entry* ce = find_call_entry(client_id); 
    if (ce == NULL) { 
        fprintf(stderr, "Maximum number of clients reached.\n"); 
        return NULL; 
    } 
    pthread_mutex_lock(&call_table_lock); 
    if (seq_number > ce->seq_number) { 
        ce->in_progress = 1; ce->seq_number = seq_number; 
        pthread_mutex_unlock(&call_table_lock); 
        if (strcmp(cmd, "idle") == 0) { 
            idle(arg1); ce->last_result = 0; 
        } else if (strcmp(cmd, "get") == 0) { 
            ce->last_result = get(arg1); 
        } else if (strcmp(cmd, "put") == 0) { 
            ce->last_result = put(arg1, arg2);
        } 
        pthread_mutex_lock(&call_table_lock); 
        ce->in_progress = 0; pthread_mutex_unlock(&call_table_lock); 
    } 
    char response[BUFLEN]; 
    if (ce->in_progress) { 
        snprintf(response, BUFLEN, "%s,ACK", cmd); 
    } else { 
        snprintf(response, BUFLEN, "%s,%d,%d,%d", cmd, client_id, ce->seq_number, ce->last_result); 
    } 
    send_packet(rpc.recv_socket, rpc.dst_addr, rpc.dst_len, response, strlen(response)); free(req_args); 
    return NULL; 
} 
int main(int argc, char* argv[]) { 
    if (argc != 2) { 
    fprintf(stderr, "Usage: %s port\n", argv[0]); 
    return 1; 
    } 
    int port = atoi(argv[1]); 
    pthread_mutex_init(&call_table_lock, NULL); 
    memset(call_table, 0, sizeof(call_table)); 
    struct rpc_connection rpc; rpc.recv_socket = init_socket(port); 
    rpc.client_id = 0; // Server doesn't need a client_id itself 
    rpc.seq_number = 0; 
    while (1) { 
        struct packet_info request = receive_packet(rpc.recv_socket); 
        struct request_handler_args* req_args = (struct request_handler_args*)malloc(sizeof(struct request_handler_args)); 
        req_args->rpc = rpc; 
        req_args->rpc.dst_addr = request.sock; 
        req_args->rpc.dst_len = request.slen; 
        strncpy(req_args->request, request.buf, BUFLEN); 
        pthread_t thread; 
        pthread_create(&thread, NULL, request_handler, (void*)req_args); 
        pthread_detach(thread); 
    } 
    RPC_close(&rpc); 
    return 0; 
}