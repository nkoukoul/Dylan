#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "utils.h"
#include "objects.h"
#include "io_context.h"
#include "strand.h"

void websocket_create_response(struct strand *sd, struct client_context *c_ctx)
{
    uint64_t index = 0;
    char response[1024];
    memset(response, 0, 1024);
    response[index++] = 0x81; //fin = 1, rsv1,2,3 = 0 text frame opcode = 1
    int payload_len = c_ctx->websocket_message_len_;
    if (c_ctx->websocket_message_len_ > 65535)
    {
        payload_len = 127;
    }
    else if (c_ctx->websocket_message_len_ > 125)
    {
        payload_len = 126;
    }
    
    response[index++] = (payload_len) & 0x7F;
    
    if (c_ctx->websocket_message_len_ > 65535)
    {
        unsigned char tmp[8];
        tmp[0] = (unsigned char)(c_ctx->websocket_message_len_);
        tmp[1] = (unsigned char)(c_ctx->websocket_message_len_ >> 8);
        tmp[2] = (unsigned char)(c_ctx->websocket_message_len_ >> 16);
        tmp[3] = (unsigned char)(c_ctx->websocket_message_len_ >> 24);
        tmp[4] = (unsigned char)(c_ctx->websocket_message_len_ >> 32);
        tmp[5] = (unsigned char)(c_ctx->websocket_message_len_ >> 40);
        tmp[6] = (unsigned char)(c_ctx->websocket_message_len_ >> 48);
        tmp[7] = (unsigned char)(c_ctx->websocket_message_len_ >> 54);
        
        memcpy(response + index, tmp, 8);
        index += 8;
    }
    else if (c_ctx->websocket_message_len_ > 125)
    {
        unsigned char tmp[2];
        tmp[0] = (unsigned char)(c_ctx->websocket_message_len_);
        tmp[1] = (unsigned char)(c_ctx->websocket_message_len_ >> 8);
        memcpy(response + index, tmp, 2);
        index += 2;
    }
    memcpy(response + index, c_ctx->websocket_message_, c_ctx->websocket_message_len_);
    c_ctx->websocket_message_len_ += index; //add protocol bytes
    memcpy(c_ctx->websocket_message_, response, c_ctx->websocket_message_len_);
    void (*func_ptr)(struct strand *, struct client_context *) = tcp_write;
    add_job(sd, c_ctx, func_ptr);
}

void websocket_parse_request(struct strand *sd, struct client_context *c_ctx)
{
    int fin, rsv1, rsv2, rsv3, opcode, mask, i = 0;
    uint64_t payload_len;
    unsigned char mask_key[4];
    uint8_t octet = (uint8_t)c_ctx->websocket_data_[i++]; //8 bit char
    fin = (octet >> 7) & 0x01;
    rsv1 = (octet >> 6) & 0x01;
    rsv2 = (octet >> 5) & 0x01;
    rsv3 = (octet >> 4) & 0x01;
    opcode = octet & 0x0F;
    octet = (uint8_t)c_ctx->websocket_data_[i++]; //8 bit char
    mask = (octet >> 7) & 0x01;
    payload_len = (octet) & 0x7F;
    if (payload_len == 126)
    {
        uint16_t octet_2 = ((uint16_t)c_ctx->websocket_data_[i]) << 8 + (uint16_t)c_ctx->websocket_data_[i+1];  
        payload_len = octet_2;
        i += 2;
    }
    else if (payload_len == 127)
    {
        uint64_t octet_8 = ((uint64_t)c_ctx->websocket_data_[i]) << 56 
                            + ((uint64_t)c_ctx->websocket_data_[i+1]) << 48
                            + ((uint64_t)c_ctx->websocket_data_[i+2]) << 40
                            + ((uint64_t)c_ctx->websocket_data_[i+3]) << 32
                            + ((uint64_t)c_ctx->websocket_data_[i+4]) << 24
                            + ((uint64_t)c_ctx->websocket_data_[i+5]) << 16
                            + ((uint64_t)c_ctx->websocket_data_[i+6]) << 8
                            + ((uint64_t)c_ctx->websocket_data_[i+7]);  
        payload_len = octet_8;
        i += 8;
    }
    if (mask == 1)
    {        
        mask_key[0] = c_ctx->websocket_data_[i]; 
        mask_key[1] = c_ctx->websocket_data_[i+1];
        mask_key[2] = c_ctx->websocket_data_[i+2];
        mask_key[3] = c_ctx->websocket_data_[i+3];
        i +=4;
    }
    char * payload = (char *)malloc((payload_len + 1) * sizeof(char));
    memcpy(payload, c_ctx->websocket_data_ + i, payload_len);
    memset(c_ctx->websocket_data_, 0, 1024);
    c_ctx->websocket_data_len_ = 0;
    for (int j = 0; j < payload_len; j++)
    {
        payload[j] = payload[j] ^ (mask_key[(j % 4)]);
    }
    payload[payload_len] = '\0';
    memcpy(c_ctx->websocket_message_, payload, payload_len); //copy without null
    c_ctx->websocket_message_len_ = payload_len;
    printf("received %s\n", payload);
    free(payload);
    void (*func_ptr)(struct strand *, struct client_context *) = websocket_create_response;
    add_job(sd, c_ctx, func_ptr);
}

#endif // WEBSOCKET_H