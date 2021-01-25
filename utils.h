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

char * get_data_from_file(char * filename, size_t * file_size)
{
  int fd = open(filename, O_RDONLY);
  if (fd < 0)
  {
    *file_size = 0;
    //printf("unable to open %s\n", filename);
    return NULL;
  }
  *file_size = lseek(fd, 0, SEEK_END);
  char * data = (char*)malloc((*file_size) * sizeof(char));
  pread(fd, data, *file_size, 0);
  close(fd);
  //printf("file to open %s with size %lu\n", filename, *file_size);
  return data;
}

#endif // UTILS_H