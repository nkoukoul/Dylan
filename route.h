#ifndef ROUTE_H
#define ROUTE_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "utils.h"

void insert_route(struct route_table * rt, char * url, char * method)
{
    size_t index = hash((unsigned char *)url) % 100;
    if (strcmp("GET", method) == 0)
    {
        rt->route_urls[index][0] = 1;
    }
    else if (strcmp("POST", method) == 0){
        rt->route_urls[index][1] = 1;
    }
}

int check_if_route_exists(struct route_table * rt, char * url, char * method)
{
    size_t index = hash((unsigned char *)url) % 100;
    if (strcmp("GET", method) == 0 && rt->route_urls[index][0])
    {
         return 1;
    }
    else if (strcmp("POST", method) == 0 && rt->route_urls[index][1])
    {
         return 1;
    }
    return 0;
}

#endif //ROUTE_H