#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "server_functions.h"
#include "client.h"
#include "udp.h"
#include <errno.h>

#define MAX_CLIENTS 100

struct call_entry
{
    int client_id;
    int seq_number;
    int last_result;
    int in_progress;
};

struct call_entry call_table[MAX_CLIENTS];

pthread_mutex_t call_table_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t wait_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t wait_cond = PTHREAD_COND_INITIALIZER;

struct call_entry *find_call_entry(int client_id)
{
    pthread_mutex_lock(&call_table_lock);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (call_table[i].client_id == client_id)
        {
            pthread_mutex_unlock(&call_table_lock);
            return &call_table[i];
        }
        else if (call_table[i].client_id == 0)
        {
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
}

struct request_handler_args
{
    struct rpc_connection rpc;
    char request[BUFLEN];
};

void *request_handler(void *args)
{
    struct request_handler_args *req_args = (struct request_handler_args *)args;
    struct rpc_connection rpc = req_args->rpc;
    char *request = req_args->request;
    int client_id, seq_number, arg1, arg2;
    char cmd[10];
    sscanf(request, "%9[^,],%d,%d,%d,%d", cmd, &client_id, &seq_number, &arg1, &arg2);
    struct call_entry *ce = find_call_entry(client_id);
    if (ce == NULL)
    {
        fprintf(stderr, "Maximum number of clients reached.\n");
        return NULL;
    }
    pthread_mutex_lock(&call_table_lock);
    if (seq_number > ce->seq_number)
    {
        ce->in_progress = 1;
        ce->seq_number = seq_number;
        pthread_mutex_unlock(&call_table_lock);
        if (strcmp(cmd, "idle") == 0)
        {
            idle(arg1);
            ce->last_result = 0;
        }
        else if (strcmp(cmd, "get") == 0)
        {
            ce->last_result = get(arg1);
        }
        else if (strcmp(cmd, "put") == 0)
        {
            ce->last_result = put(arg1, arg2);
        }
        pthread_mutex_lock(&call_table_lock);
        ce->in_progress = 0;
        pthread_mutex_unlock(&call_table_lock);
        pthread_mutex_lock(&wait_lock);
        pthread_cond_broadcast(&wait_cond);
        pthread_mutex_unlock(&wait_lock);
    }
    char response[BUFLEN];
    if (ce->in_progress)
    {
        pthread_mutex_lock(&wait_lock);
        while (ce->in_progress)
        {
            pthread_cond_wait(&wait_cond, &wait_lock);
        }
        pthread_mutex_unlock(&wait_lock);
    }
    snprintf(response, BUFLEN, "%d,%d,%d", seq_number, ce->last_result, errno);
    rpc_send(rpc, response);
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s port\n", argv[0]);
        exit(1);
    }
    int port = atoi(argv[1]);
    if (port == 0)
    {
        fprintf(stderr, "Invalid port number.\n");
        exit(1);
    }
    struct socket server_socket = init_socket(port);
    printf("Server started on port %d.\n", port);
    while (1)
    {
        struct packet_info packet = receive_packet(server_socket);
        if (fork() == 0)
        {
            struct rpc_connection rpc;
            rpc_init(&rpc, packet.sock);
            char request[BUFLEN];
            rpc_recv(&rpc, request);
            struct request_handler_args args = {rpc, request};
            request_handler(&args);
            rpc_close(&rpc);
            exit(0);
        }
    }
    close_socket(server_socket);
    return 0;
}