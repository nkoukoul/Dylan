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
#include "strand.h"

#define MAXBUF 1024

struct io_context
{
    int server_sock_;
};

struct client_context
{
    int client_socket_;
    char *data_;
};

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

void http_request_parse(struct strand *sd, struct client_context *c_ctx)
{
    char request_type[4], url[20], protocol[10], *line, *line_end, *line_start;
    char nl_del[] = "\r\n\r\n";
    char sp_del[] = " ";
    //first line
    line_start = c_ctx->data_;
    line_end = strstr(line_start, "\r\n\r\n");
    /* walk through other tokens */
    while (line_end != NULL)
    {
        int length = line_end - line_start + 1;
        line = (char *)malloc(length * sizeof(char));
        memcpy(line, line_start, length);
        line[length] = '\0';
        printf(" %s\n", line);
        line_start = line_end + 1;
        line_end = strstr(line_start, "\r\n\r\n");
    }
}

void do_read(struct strand *sd, struct client_context *c_ctx)
{
    char inbuffer[MAXBUF], *p = inbuffer;

    // Read data from client
    int bytes_read = read(c_ctx->client_socket_, inbuffer, MAXBUF);
    if (bytes_read <= 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            //client read block try again
            void (*func_ptr)(struct strand *, struct client_context *) = do_read;
            add_job(sd, c_ctx, func_ptr);
            return;
        }
        else
        {
            //client closed connection
            close(c_ctx->client_socket_);
            return;
        }
    }
    else
    {
        printf("read bytes %d\n", bytes_read);
        printf("%s", inbuffer);
        c_ctx->data_ = (char *)malloc(bytes_read * sizeof(char));
        memcpy(c_ctx->data_, inbuffer, bytes_read);
        void (*func_ptr)(struct strand *, struct client_context *) = http_request_parse;
        add_job(sd, c_ctx, func_ptr);
    }
}

void handle_connections(struct strand *sd, struct client_context *c_ctx)
{
    struct sockaddr_in client;
    socklen_t client_len;

    client_len = sizeof client;
    int client_socket = accept(sd->ioc_->server_sock_, (struct sockaddr *)&client, &client_len);
    if (client_socket < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            ;
            ; //no incoming connection for non-blocking sockets
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
        c_ctx = (struct client_context *)malloc(sizeof(struct client_context));
        c_ctx->client_socket_ = client_socket;
        c_ctx->data_ = NULL;
        void (*func_ptr)(struct strand *, struct client_context *) = do_read;
        add_job(sd, c_ctx, func_ptr);
    }
    void (*func_ptr)(struct strand *, struct client_context *) = handle_connections;
    add_job(sd, NULL, func_ptr);
}

void http_handler(struct io_context *ioc, char *ipaddr, int port)
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

struct io_context *initialize_io_context(char *ipaddr, int port)
{
    struct io_context *ioc = (struct io_context *)malloc(sizeof(struct io_context));
    http_handler(ioc, ipaddr, port);
    return ioc;
}

#endif //IO_CONTEXT_H