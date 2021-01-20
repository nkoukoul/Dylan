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
#include "utils.h"

#define MAXBUF 1024

//forward declarations

struct http_response;
struct http_request;
void clear_http_request(struct http_request * http_req);
void clear_http_response(struct http_response * http_res);

struct server_context
{
    int server_sock_;
};

struct client_context
{
    int client_socket_;
    char *data_;
    struct http_request *http_req_;
    struct http_response *http_res_;
};

void clear_client_context(struct client_context * c_ctx)
{
    free(c_ctx->data_);
    c_ctx->data_ = NULL;
    clear_http_request(c_ctx->http_req_);
    free(c_ctx->http_req_);
    c_ctx->http_req_ = NULL;
    clear_http_response(c_ctx->http_res_);
    free(c_ctx->http_res_);
    c_ctx->http_res_ = NULL;
    free(c_ctx);
}

struct http_request
{
    char *request_type_;
    char *url_;
    char *protocol_;
    char *header_values[100];
    char *header_keys[100];
};

void clear_http_request(struct http_request * http_req)
{
    free(http_req->request_type_);
    http_req->request_type_ = NULL;
    free(http_req->url_);
    http_req->url_ = NULL;
    free(http_req->protocol_);
    http_req->protocol_ = NULL;
    for (int i = 0; i < 100; i++)
    {
        free(http_req->header_values[i]);
        http_req->header_values[i] = NULL;
        free(http_req->header_keys[i]);
        http_req->header_keys[i] = NULL;
    }
}

struct http_response
{
    char *response_;
    int res_size_;
};

void clear_http_response(struct http_response * http_res)
{
    free(http_res->response_);
    http_res->response_ = NULL;
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

void http_write(struct strand *sd, struct client_context *c_ctx)
{
    write(c_ctx->client_socket_, c_ctx->http_res_->response_, c_ctx->http_res_->res_size_);
    //close socket if http
    close(c_ctx->client_socket_);
    clear_client_context(c_ctx);
}

void http_create_response(struct strand *sd, struct client_context *c_ctx)
{
    char *response;
    response = (char *)malloc(1024 * sizeof(char));
    int cx = 0, tcx = 0;
    tcx += snprintf (response + tcx, 1024 - tcx, "HTTP/1.1 200 OK\r\n");
    char * daytime = get_daytime();
    tcx += snprintf (response + tcx, 1024 - tcx, "Date %s\r\n",  daytime);
    free(daytime);
    tcx += snprintf (response + tcx, 1024 - tcx, "Connection: close\r\n");
    tcx += snprintf (response + tcx, 1024 - tcx, "\r\n");
    c_ctx->http_res_ = (struct http_response *)calloc(1, sizeof(struct http_response));
    c_ctx->http_res_->response_ = response;
    c_ctx->http_res_->res_size_ = tcx;
    void (*func_ptr)(struct strand *, struct client_context *) = http_write;
    add_job(sd, c_ctx, func_ptr);
}

void http_parse_request(struct strand *sd, struct client_context *c_ctx)
{
    c_ctx->http_req_ = (struct http_request *)calloc(1, sizeof(struct http_request));
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
        //printf("%s", line);
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
                    c_ctx->http_req_->request_type_ = (char *)malloc(token_length * sizeof(char));
                    memcpy(c_ctx->http_req_->request_type_, token_start, token_length - 1);
                    c_ctx->http_req_->request_type_[token_length - 1] = '\0';
                    //printf("%s\n", c_ctx->http_req_->request_type_);
                    token_start = token_end + 1;
                    token_end = strstr(token_start, " ");
                    break;
                case 2:
                    c_ctx->http_req_->url_ = (char *)malloc(token_length * sizeof(char));
                    memcpy(c_ctx->http_req_->url_, token_start, token_length - 1);
                    c_ctx->http_req_->url_[token_length - 1] = '\0';
                    //printf("%s\n", c_ctx->http_req_->url_);
                    token_start = token_end + 1;
                    token_end = strstr(token_start, "\n");
                    break;
                case 3:
                    c_ctx->http_req_->protocol_ = (char *)malloc(token_length * sizeof(char));
                    memcpy(c_ctx->http_req_->protocol_, token_start, token_length - 1);
                    c_ctx->http_req_->protocol_[token_length - 1] = '\0';
                    //printf("%s\n", c_ctx->http_req_->protocol_);
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
            token_end = strstr(token_start, ": ");
            if (token_end)
            { //key
                int token_length = token_end - token_start + 1;
                token = (char *)malloc(token_length * sizeof(char));
                memcpy(token, token_start, token_length - 1);
                token[token_length - 1] = '\0';
                //printf("key %s\n", token);
                int index = hash((unsigned char *)token) % 100;
                c_ctx->http_req_->header_keys[index] = (char *)malloc(token_length * sizeof(char));
                memcpy(c_ctx->http_req_->header_keys[index], token_start, token_length - 1);
                c_ctx->http_req_->header_keys[index][token_length - 1] = '\0';
                free(token);
                token_start = token_end + 2; //skip 2 chars
                token_end = strstr(token_start, "\n");
                if (token_end)
                { //value
                    token_length = token_end - token_start + 1;
                    c_ctx->http_req_->header_values[index] = (char *)malloc(token_length * sizeof(char));
                    memcpy(c_ctx->http_req_->header_values[index], token_start, token_length - 1);
                    c_ctx->http_req_->header_values[index][token_length - 1] = '\0';
                    //printf("value %s\n", token);
                }
            }
        }
        free(line);
        line_num++;
        line_start = line_end + 2; //skip two characters
        line_end = strstr(line_start, "\r\n");
    }
    void (*func_ptr)(struct strand *, struct client_context *) = http_create_response;
    add_job(sd, c_ctx, func_ptr);
}

void http_read(struct strand *sd, struct client_context *c_ctx)
{
    char inbuffer[MAXBUF], *p = inbuffer;

    // Read data from client
    int bytes_read = read(c_ctx->client_socket_, inbuffer, MAXBUF);
    if (bytes_read <= 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            //client read block try again
            void (*func_ptr)(struct strand *, struct client_context *) = http_read;
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
        //printf("read bytes %d\n", bytes_read);
        //printf("%s", inbuffer);
        c_ctx->data_ = (char *)malloc(bytes_read * sizeof(char));
        memcpy(c_ctx->data_, inbuffer, bytes_read);
        void (*func_ptr)(struct strand *, struct client_context *) = http_parse_request;
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
        c_ctx = (struct client_context *)malloc(sizeof(struct client_context));
        c_ctx->client_socket_ = client_socket;
        c_ctx->data_ = NULL;
        void (*func_ptr)(struct strand *, struct client_context *) = http_read;
        add_job(sd, c_ctx, func_ptr);
    }
    void (*func_ptr)(struct strand *, struct client_context *) = handle_connections;
    add_job(sd, NULL, func_ptr);
}

void http_handler(struct server_context *ioc, char *ipaddr, int port)
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

struct server_context *initialize_io_context(char *ipaddr, int port)
{
    struct server_context *ioc = (struct server_context *)malloc(sizeof(struct server_context));
    http_handler(ioc, ipaddr, port);
    return ioc;
}

#endif //IO_CONTEXT_H