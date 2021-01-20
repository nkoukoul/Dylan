#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <time.h>

unsigned long hash(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

char * get_daytime(){
  time_t rawtime;
  struct tm * timeinfo;
  char *buffer = (char *)malloc(80 * sizeof(char));
  
  time (&rawtime);
  timeinfo = localtime(&rawtime);
  
  strftime(buffer,80,"%a %b %d %H:%M:%S %Y",timeinfo);
  return buffer;
}


#endif // UTILS_H