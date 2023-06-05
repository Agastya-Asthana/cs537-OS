/*#include "client.h"
#include "udp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct rpc_connection RPC_init(int src_port, int dst_port, char dst_addr[]) {
    struct rpc_connection rpc;

    // initialize the receive socket
    rpc.recv_socket = init_socket(src_port);

    // populate destination address
    struct sockaddr_in dst_addr_in;
    memset(&dst_addr_in, 0, sizeof(dst_addr_in));
    dst_addr_in.sin_family = AF_INET;
    dst_addr_in.sin_port = htons(dst_port);
    if (inet_aton(dst_addr, &dst_addr_in.sin_addr) == 0) {
        die("inet_aton() failed\n");
    }
    memcpy(&rpc.dst_addr, &dst_addr_in, sizeof(rpc.dst_addr));
    rpc.dst_len = sizeof(rpc.dst_addr);

    // initialize the sequence number
    rpc.seq_number = 0;

    // initialize the client ID
    srand(time(NULL));
    rpc.client_id = rand() % 100000;

    return rpc;
}

void RPC_idle(struct rpc_connection *rpc, int time) {
    idle(time);
}

int RPC_get(struct rpc_connection *rpc, int key) {
    int result = -1;
    int retries = 0;
    while (retries < RETRY_COUNT) {
        char payload[BUFLEN];
        sprintf(payload, "GET %d %d %d", rpc->client_id, rpc->seq_number++, key);
        send_packet(rpc->recv_socket, (struct sockaddr)rpc->dst_addr, rpc->dst_len, payload, strlen(payload));
        struct packet_info packet = receive_packet_timeout(rpc->recv_socket, TIMEOUT_TIME);
        if (packet.recv_len > 0) {
            char *response = strtok(packet.buf, " ");
            int server_id = atoi(strtok(NULL, " "));
            int response_seq = atoi(strtok(NULL, " "));
            if (strcmp(response, "OK") == 0 && server_id == rpc->client_id && response_seq == rpc->seq_number-1) {
                result = atoi(strtok(NULL, " "));
                break;
            }
        }
        retries++;
    }
    return result;
}

int RPC_put(struct rpc_connection *rpc, int key, int value) {
    int result = -1;
    int retries = 0;
    while (retries < RETRY_COUNT) {
        char payload[BUFLEN];
        sprintf(payload, "PUT %d %d %d %d", rpc->client_id, rpc->seq_number++, key, value);
        send_packet(rpc->recv_socket, (struct sockaddr)rpc->dst_addr, rpc->dst_len, payload, strlen(payload));
        struct packet_info packet = receive_packet_timeout(rpc->recv_socket, TIMEOUT_TIME);
        if (packet.recv_len > 0) {
            char *response = strtok(packet.buf, " ");
            int server_id = atoi(strtok(NULL, " "));
            int response_seq = atoi(strtok(NULL, " "));
            if (strcmp(response, "OK") == 0 && server_id == rpc->client_id && response_seq == rpc->seq_number-1) {
                result = atoi(strtok(NULL, " "));
                break;
            }
        }
        retries++;
    }
    return result;
}

void RPC_close(struct rpc_connection *rpc) {
    close_socket(rpc->recv_socket);
}*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "client.h"
#include "udp.h"

struct rpc_connection RPC_init(int src_port, int dst_port, char dst_addr[])
{
    struct rpc_connection conn;
    conn.recv_socket = init_socket(src_port);
    populate_sockaddr(AF_INET, dst_port, dst_addr, (struct sockaddr_storage *)&conn.dst_addr, &conn.dst_len);
    conn.seq_number = 0;
    conn.client_id = rand() % 1000000;

    return conn;
}

void handle_ack(struct rpc_connection *rpc, struct packet_info *packet)
{
    int seq_number;
    sscanf(packet->buf, "%d", &seq_number);

    if (seq_number == rpc->seq_number)
    {
        usleep(TIMEOUT_TIME * 1000 * 1000);
    }
}

void RPC_idle(struct rpc_connection *rpc, int time)
{
    char request[BUFLEN];
    snprintf(request, BUFLEN, "idle %d %d %d", rpc->client_id, rpc->seq_number, time);

    int retries = 0;
    while (retries < RETRY_COUNT)
    {
        send_packet(rpc->recv_socket, (struct sockaddr)rpc->dst_addr, rpc->dst_len, request, strlen(request));

        struct packet_info packet = receive_packet_timeout(rpc->recv_socket, TIMEOUT_TIME);
        if (packet.recv_len > 0)
        {
            if (strncmp(packet.buf, "ack", 3) == 0)
            {
                handle_ack(rpc, &packet);
            }
            else if (strncmp(packet.buf, "idle_done", 9) == 0)
            {
                rpc->seq_number++;
                break;
            }
        }
        else
        {
            retries++;
        }
    }

    if (retries == RETRY_COUNT)
    {
        fprintf(stderr, "Error: server did not respond after % d retries\n", RETRY_COUNT);
        exit(1);
    }
}

int RPC_get(struct rpc_connection *rpc, int key)
{
    char request[BUFLEN];
    snprintf(request, BUFLEN, "get %d %d %d", rpc->client_id, rpc->seq_number, key);

    int retries = 0;
    while (retries < RETRY_COUNT)
    {
        send_packet(rpc->recv_socket, (struct sockaddr)rpc->dst_addr, rpc->dst_len, request, strlen(request));

        struct packet_info packet = receive_packet_timeout(rpc->recv_socket, TIMEOUT_TIME);
        if (packet.recv_len > 0)
        {
            if (strncmp(packet.buf, "ack", 3) == 0)
            {
                handle_ack(rpc, &packet);
            }
            else
            {
                int seq_number, value;
                sscanf(packet.buf, "% d % d", &seq_number, &value);

                if (seq_number == rpc->seq_number)
                {
                    rpc->seq_number++;
                    return value;
                }
            }
        }
        else
        {
            retries++;
        }
    }

    if (retries == RETRY_COUNT)
    {
        fprintf(stderr, "Error: server did not respond after % d retries\n", RETRY_COUNT);
        exit(1);
    }

    return -1;
}

int RPC_put(struct rpc_connection *rpc, int key, int value)
{
    char request[BUFLEN];
    snprintf(request, BUFLEN, "put % d % d % d % d", rpc->client_id, rpc->seq_number, key, value);

    int retries = 0;
    while (retries < RETRY_COUNT)
    {
        send_packet(rpc->recv_socket, (struct sockaddr)rpc->dst_addr, rpc->dst_len, request, strlen(request));

        struct packet_info packet = receive_packet_timeout(rpc->recv_socket, TIMEOUT_TIME);
        if (packet.recv_len > 0)
        {
            if (strncmp(packet.buf, "ack", 3) == 0)
            {
                handle_ack(rpc, &packet);
            }
            else
            {
                int seq_number, status;
                sscanf(packet.buf, "% d % d", &seq_number, &status);

                if (seq_number == rpc->seq_number)
                {
                    rpc->seq_number++;
                    return status;
                }
            }
        }
        else
        {
            retries++;
        }
    }

    if (retries == RETRY_COUNT)
    {
        fprintf(stderr, "Error: server did not respond after % d retries\n", RETRY_COUNT);
        exit(1);
    }

    return -1;
}

void RPC_close(struct rpc_connection *rpc)
{
    close_socket(rpc->recv_socket);
}