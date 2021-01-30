#ifndef HTTP_H
#define HTTP_H
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "utils.h"
#include "route.h"
#include "objects.h"
#include "io_context.h"
#include "strand.h"

void tcp_write(struct strand *sd, struct client_context *c_ctx);

void http_create_response(struct strand *sd, struct client_context *c_ctx)
{
    size_t file_size = 0;
    char * file_data = NULL;
    char status[50];
    memset(status, 0, 50);
    char sec_websocket_key[50];
    memset(sec_websocket_key, 0, 50);
    char *routename_start = c_ctx->http_req_->url_ + 1; // skip initial /
    char *routename_end = strstr(routename_start, "/");
    char *route = NULL;
    if (routename_end)
    {
        size_t routename_length = routename_end - routename_start +2;
        route = (char *)malloc(routename_length * sizeof(char));
        memcpy(route, c_ctx->http_req_->url_, routename_length - 1);
        route[routename_length - 1] = '\0';
        if (check_if_route_exists(sd->ioc_->rt_, route, c_ctx->http_req_->request_type_))
        {
            // check for websocket request
            int con_index = hash("Connection") % 100;
            int upg_index = hash("Upgrade") % 100;
            if (c_ctx->http_req_->header_keys[con_index] && strcmp(c_ctx->http_req_->header_values[con_index], "Upgrade") == 0 && 
                c_ctx->http_req_->header_keys[upg_index] && strcmp(c_ctx->http_req_->header_values[upg_index], "websocket") == 0)
            {
                int key_index = hash("Sec-WebSocket-Key") % 100;
                char * key = c_ctx->http_req_->header_values[key_index];
                int key_len = 0;
                char c;
                while (c = *key++)
                {
                    key_len++;
                }
                int output_key_len = 0;
                char *encoded_key = generate_ws_key(c_ctx->http_req_->header_values[key_index],
                                                    key_len, &output_key_len);
                snprintf(sec_websocket_key, 50, "%s", encoded_key);
                free(encoded_key);
                c_ctx->is_websocket_ = 1;
                snprintf(status, 50, "%s", "101 Switching Protocols");
            }
            else
            {
                char *filename_start = routename_end + 1;
                size_t filename_legth = c_ctx->http_req_->url_length_ - routename_length;
                char *filename = (char *)malloc(filename_legth * (sizeof(char)));
                memcpy(filename, filename_start, filename_legth);
                get_data_from_file(filename, &file_data, &file_size);
                if (file_size)
                {
                    snprintf(status, 50, "%s", "200 OK");
                }
                else
                {
                    free(file_data);
                    snprintf(status, 50, "%s", "400 Bad Request");
                }
                free(filename);
            }
        }
        else
        {
        snprintf(status, 50, "%s", "400 Bad Request");
        }
    }
    else
    {
        snprintf(status, 50, "%s", "400 Bad Request");
    }
    free(route);
    c_ctx->http_res_ = (struct http_response *)calloc(1, sizeof(struct http_response));
    size_t response_size = 1024 + file_size;
    c_ctx->http_res_->response_ = (char *)calloc(response_size, sizeof(char));    
    char * response = c_ctx->http_res_->response_;
    int cx = 0, tcx = 0;
    tcx += snprintf (response + tcx, response_size - tcx, "HTTP/1.1 %s\r\n", status);
    char * daytime = get_daytime();
    tcx += snprintf (response + tcx, response_size - tcx, "Date %s\r\n",  daytime);
    free(daytime);
    if (c_ctx->is_websocket_){
        tcx += snprintf (response + tcx, response_size - tcx, "Upgrade: websocket\r\n");
        tcx += snprintf (response + tcx, response_size - tcx, "Connection: Upgrade\r\n");
        tcx += snprintf (response + tcx, response_size - tcx, "Sec-WebSocket-Accept: %s\r\n", sec_websocket_key);
        c_ctx->close_connection_ = 0;
    }
    else{
        tcx += snprintf (response + tcx, response_size - tcx, "Connection: close\r\n");
        c_ctx->close_connection_ = 1;
    }
    if (file_size)
    {
        tcx += snprintf (response + tcx, response_size - tcx, "Content-Type: text/html\r\n");
        tcx += snprintf (response + tcx, response_size - tcx, "Content-Length: %lu\r\n", file_size + 2);
    }
    tcx += snprintf (response + tcx, 1024 - tcx, "\r\n");
    if (file_size)
    {
        tcx += snprintf (response + tcx, response_size - tcx, "%s\r\n", file_data);
        free(file_data);
    }
    c_ctx->http_res_->res_size_ = tcx;
    void (*func_ptr)(struct strand *, struct client_context *) = tcp_write;
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
                    c_ctx->http_req_->url_length_ = token_length;
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

#endif //HTTP_H