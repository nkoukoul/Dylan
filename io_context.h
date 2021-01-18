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

struct http_request
{
    char *request_type_;
    char *url_;
    char *protocol_;
    char * headers_[100];
};

unsigned long
hash(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

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

    struct http_request *http_req = (struct http_request *)malloc(sizeof(struct http_request));
    char *token, *token_start, *token_end, *line, *line_end, *line_start;
    //first line
    line_start = c_ctx->data_;
    line_end = strstr(line_start, "\r\n");
    /* walk through other tokens */
    int line_num = 1;
    while (line_end != NULL)
    {
        int token_length = 0;
        int length = line_end - line_start + 2;
        line = (char *)malloc(length * sizeof(char));
        memcpy(line, line_start, length - 2);
        line[length - 2] = '\n';
        line[length - 1] = '\0';
        printf(" %s", line);
        if (line_num == 1)
        {
            int token_num = 1;
            token_start = line;
            token_end = strstr(token_start, " ");
            while (token_end != NULL)
            {
                token_length = token_end - token_start + 1;
                switch (token_num)
                {
                case 1:
                    http_req->request_type_ = (char *)malloc(token_length * sizeof(char));
                    memcpy(http_req->request_type_, token_start, token_length - 1);
                    http_req->request_type_[token_length - 1] = '\0';
                    printf(" %s\n", http_req->request_type_);
                    token_start = token_end + 1;
                    token_end = strstr(token_start, " ");
                    break;
                case 2:
                    http_req->url_ = (char *)malloc(token_length * sizeof(char));
                    memcpy(http_req->url_, token_start, token_length - 1);
                    http_req->url_[token_length - 1] = '\0';
                    printf(" %s\n", http_req->url_);
                    token_start = token_end + 1;
                    token_end = strstr(token_start, "\n");
                    break;
                case 3:
                    http_req->protocol_ = (char *)malloc(token_length * sizeof(char));
                    memcpy(http_req->protocol_, token_start, token_length - 1);
                    http_req->protocol_[token_length - 1] = '\0';
                    printf(" %s\n", http_req->protocol_);
                }
                token_num++;
                if (token_num == 4)
                {
                    break;
                }
            }
        }
        else
        {
            token_start = line;
            token_end = strstr(token_start, " ");
            if (token_end)
            {//key
                int token_length = token_end - token_start + 1;
                token = (char *)malloc(token_length * sizeof(char));
                memcpy(token, token_start, token_length -1);
                token[token_length - 1] = '\0';
                //printf("key %s\n", token);
                int index = hash((unsigned char *)token)%100;                
                free(token);
                token_start = token_end + 1;
                token_end = strstr(token_start, "\n");
                if (token_end)
                {//value
                    token_length = token_end - token_start + 1;
                    http_req->headers_[index] = (char *)malloc(token_length * sizeof(char));
                    memcpy(http_req->headers_[index], token_start, token_length - 1);
                    http_req->headers_[index][token_length - 1] = '\0';
                    //printf("value %s\n", token);
                }
            }
        }
        line_num++;
        line_start = line_end + 1;
        line_end = strstr(line_start, "\r\n");
    }
    for (int i = 0; i < 100; i++)
    {
        if (http_req->headers_[i])
        {
            printf("value: %s\n", http_req->headers_[i]);
        }
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