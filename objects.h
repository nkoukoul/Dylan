#ifndef OBJECTS_H
#define OBJECTS_H
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define MAXBUF 1024

struct strand;

struct route_table
{
    int route_urls[100][2]; //only get and post for time being
};

struct route_table * create_route_table()
{
    struct route_table *rt = (struct route_table *) calloc(1, sizeof(struct route_table));
    return rt;
}

struct http_request
{
    size_t url_length_;
    char *request_type_;
    char *url_;
    char *protocol_;
    char *header_values[100];
    char *header_keys[100];
};

void clear_http_request(struct http_request * http_req)
{
    free(http_req->request_type_);
    free(http_req->url_);
    free(http_req->protocol_);
    for (int i = 0; i < 100; i++)
    {
        free(http_req->header_values[i]);
        free(http_req->header_keys[i]);
    }
    free(http_req);
}

struct http_response
{
    char *response_;
    int res_size_;
};

void clear_http_response(struct http_response * http_res)
{
    free(http_res->response_);
    free(http_res);
}

struct server_context
{
    int server_sock_;
    struct route_table *rt_;
};

struct client_context
{
    int client_socket_;
    int close_connection_;
    int is_websocket_;
    int websocket_handshake_copleted_;
    uint64_t websocket_data_len_;
    uint64_t websocket_message_len_;
    char data_[MAXBUF]; //at the moment 1024 bytes supported only
    char websocket_data_[MAXBUF]; //at the moment 1024 bytes supported only
    char websocket_message_[MAXBUF];
    struct http_request *http_req_;
    struct http_response *http_res_;
};

void clear_client_context(struct client_context * c_ctx)
{
    //free(c_ctx->data_);
    clear_http_request(c_ctx->http_req_);
    clear_http_response(c_ctx->http_res_);
    free(c_ctx);
}

struct job
{
    int job_id_;
    struct job *next_;
    struct client_context *c_ctx_;
    void (*func_ptr_)(struct strand *, struct client_context *);
};

void clear_job(struct job* jb)
{
    jb->next_ = NULL;
    jb->c_ctx_ = NULL;//client context will be free when request is complete
    free(jb);
}

struct strand
{
    struct job *cursor_;
    pthread_mutex_t strand_lock_;
    int size_;
    int job_id_;
    struct server_context *ioc_;
};


#endif //OBJECTS_H