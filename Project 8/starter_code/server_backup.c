#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>

#include "udp.h"
#include "server_functions.h"

#define MAX_THREADS 10

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct key_val_pair{
    int key;
    int value;
};

struct server_args {
    struct socket socket;
    int id;
    struct key_val_pair kv_store[NUMKEYS];
};

void* handle_client(void *args){
    struct server_args* srv_args = (struct server_args*) args;

    struct packet_info packet = receive_packet(srv_args->socket);

    while(packet.buf[0] != 'Q'){
        int key, value;

        if(packet.buf[0] == 'G'){
            sscanf(packet.buf, "G,%d", &key);
            value = get(key);
            char response[BUFLEN];
            snprintf(response, BUFLEN, "%d", value);
            send_packet(srv_args->socket, packet.sock, packet.slen, response, strlen(response) + 1);
        }
        else if(packet.buf[0] == 'P'){
            sscanf(packet.buf, "P,%d,%d", &key, &value);
            int status = put(key, value);
            char response[BUFLEN];
            snprintf(response, BUFLEN, "%d", status);
            send_packet(srv_args->socket, packet.sock, packet.slen, response, strlen(response) + 1);
        }
        packet = receive_packet(srv_args->socket);
    }
    //RPC_close(srv_args->rpc);
    close(srv_args->socket.fd);
    pthread_mutex_lock(&mutex);
    printf("Server %d: Connection closed\n", srv_args->id);
    pthread_mutex_unlock(&mutex);
    free(srv_args);
    return NULL;
}

int main(int argc, char *argv[]){
    if(argc != 2){
        printf("Usage: %s port\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    struct socket server_socket = init_socket(port);

    pthread_t threads[MAX_THREADS];
    int num_threads = 0;

    while(true){
        pthread_mutex_lock(&mutex);
        printf("Server: Waiting for connection...\n");
        pthread_mutex_unlock(&mutex);

        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_fd = accept(server_socket.fd, (struct sockaddr*) &client_addr, &client_addr_len);
        if(client_fd == -1){
            perror("Error accepting connection");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);

        pthread_mutex_lock(&mutex);
        printf("Server: Connection accepted from %s:%d\n", client_ip, client_port);
        pthread_mutex_unlock(&mutex);

        struct server_args *args = (struct server_args *)malloc(sizeof(struct server_args));
        args->socket.fd = client_fd;
        args->socket.si = server_socket.si;
        args->id = num_threads;

        for(int i = 0; i < NUMKEYS; i++){
            args->kv_store[i].key = -1;
            args->kv_store[i].value = 0;
        }

        int ret = pthread_create(&threads[num_threads], NULL, handle_client, args);
        if(ret){
            perror("Error creating thread");
            close(client_fd);
            free(args);
            continue;
        }

        num_threads++;
        if(num_threads == MAX_THREADS){
            pthread_mutex_lock(&mutex);
            printf("Server: Maximum number of threads reached\n");
            pthread_mutex_unlock(&mutex);
            break;
        }
    }

    for(int i = 0; i < num_threads; i++){
        pthread_join(threads[i], NULL);
    }

    close_socket(server_socket);
    return 0;
}

