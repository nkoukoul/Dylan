#ifndef IO_CONTEXT_H
#define IO_CONTEXT_H
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "objects.h"
#include "route.h"
#include "http.h"
#include "websocket.h"

void non_block_socket(int sd)
{
    /* set O_NONBLOCK on fd */
    int flags = fcntl(sd, F_GETFL, 0);
    if (flags == -1)
    {
        //perror("fcntl()");
        return;
    }
    if (fcntl(sd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        //perror("fcntl()");
    }
}

void tcp_read(struct strand *sd, struct client_context *c_ctx)
{
    char inbuffer[MAXBUF], *p = inbuffer;

    // Read data from client
    int bytes_read = read(c_ctx->client_socket_, inbuffer, MAXBUF);
    if (bytes_read <= 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            //client read block try again
            void (*func_ptr)(struct strand *, struct client_context *) = tcp_read;
            add_job(sd, c_ctx, func_ptr);
            return;
        }
        else
        {
            //client closed connection
            close(c_ctx->client_socket_);
            clear_client_context(c_ctx);
            return;
        }
    }
    else
    {
        if (c_ctx->is_websocket_ && c_ctx->websocket_handshake_copleted_)
        {
            memcpy(c_ctx->websocket_data_ + c_ctx->websocket_data_len_, inbuffer, bytes_read);
            c_ctx->websocket_data_len_ += bytes_read;
            void (*func_ptr)(struct strand *, struct client_context *) = websocket_parse_request;
            add_job(sd, c_ctx, func_ptr);
        }
        else
        {
            memcpy(c_ctx->data_, inbuffer, bytes_read);
            void (*func_ptr)(struct strand *, struct client_context *) = http_parse_request;
            add_job(sd, c_ctx, func_ptr);
        }
    }
}

void tcp_write(struct strand *sd, struct client_context *c_ctx)
{
    int status;
    if (c_ctx->is_websocket_ && c_ctx->websocket_handshake_copleted_)
    {
        status = write(c_ctx->client_socket_, c_ctx->websocket_message_, c_ctx->websocket_message_len_);
        memset(c_ctx->websocket_message_, 0, 1024);
    }
    else
    {
        status = write(c_ctx->client_socket_, c_ctx->http_res_->response_, c_ctx->http_res_->res_size_);
        if(c_ctx->is_websocket_)
        {
            c_ctx->websocket_handshake_copleted_ = 1;
        }
    }

    if (c_ctx->close_connection_ || status <= 0)
    {
        //close socket on error or if desided
        close(c_ctx->client_socket_);
        clear_client_context(c_ctx);
    }
    else
    {
        if(c_ctx->is_websocket_)
        {
            void (*func_ptr)(struct strand *, struct client_context *) = tcp_read;
            add_job(sd, c_ctx, func_ptr);
        }
    }
}

void accept_connections(struct strand *sd, struct client_context *c_ctx)
{
    struct sockaddr_in client;
    socklen_t client_len;

    client_len = sizeof client;
    int client_socket = accept(sd->ioc_->server_sock_, (struct sockaddr *)&client, &client_len);
    if (client_socket < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            //no incoming connection for non-blocking sockets try again
            ;
        }
        else
        {
            printf("Error while accepting connection\n");
            exit(1);
        }
    }
    else
    {
        non_block_socket(client_socket);
        c_ctx = (struct client_context *)calloc(1, sizeof(struct client_context));
        c_ctx->client_socket_ = client_socket;
        void (*func_ptr)(struct strand *, struct client_context *) = tcp_read;
        add_job(sd, c_ctx, func_ptr);
    }
    void (*func_ptr)(struct strand *, struct client_context *) = accept_connections;
    add_job(sd, NULL, func_ptr);
}

void tcp_handler(struct server_context *ioc, char *ipaddr, int port)
{
    struct sockaddr_in server;

    ioc->server_sock_ = socket(AF_INET, SOCK_STREAM, PF_UNSPEC);
    non_block_socket(ioc->server_sock_);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(ipaddr);
    server.sin_port = htons(port);

    if (bind(ioc->server_sock_, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        printf("bind failed\n");
        exit(1);
    }
    listen(ioc->server_sock_, 5);
    printf("server accepting connections on %s : %d\n", ipaddr, port);
}

struct server_context *initialize_io_context(char *ipaddr, int port, struct route_table *rt)
{
    struct server_context *ioc = (struct server_context *)calloc(1, sizeof(struct server_context));
    ioc->rt_ = rt;
    tcp_handler(ioc, ipaddr, port);
    return ioc;
}

#endif //IO_CONTEXT_H