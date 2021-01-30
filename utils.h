#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <time.h>
#include <openssl/sha.h>

unsigned long hash(unsigned char *str)
{
  unsigned long hash = 5381;
  int c;

  while (c = *str++)
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

  return hash;
}

char *get_daytime()
{
  time_t rawtime;
  struct tm *timeinfo;
  char *buffer = (char *)malloc(80 * sizeof(char));

  time(&rawtime);
  timeinfo = localtime(&rawtime);

  strftime(buffer, 80, "%a %b %d %H:%M:%S %Y", timeinfo);
  return buffer;
}

char * get_data_from_file(char *filename, size_t *file_size)
{
  int fd = open(filename, O_RDONLY);
  if (fd < 0)
  {
    *file_size = 0;
    //printf("unable to open %s\n", filename);
    return NULL;
  }
  *file_size = lseek(fd, 0, SEEK_END);
  char *data = (char *)malloc((*file_size) * sizeof(char));
  pread(fd, data, *file_size, 0);
  close(fd);
  return data;
  //printf("file to open %s with size %lu\n", filename, *file_size);
}

unsigned int binary_to_decimal(char *binary_num, int binary_length)
{
  unsigned int dec_value = 0;
  unsigned int base = 1;
  for (int i = binary_length - 1; i >= 0; i--)
  {
      if (binary_num[i] == '1')
      {
        dec_value += base;
      }
      base = base * 2;
  }

  return dec_value;
}

char * base_64_encode(char *input, int input_length, int * output_length)
{
  char base64_lookup[] = {
      'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
      'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
      'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
      'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
      'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
      'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
      'w', 'x', 'y', 'z', '0', '1', '2', '3',
      '4', '5', '6', '7', '8', '9', '+', '/'};

  int mod_table[] = {0, 2, 1};
  *output_length = 4 * ((input_length + 2) / 3);

  char *encoded_data = (char *)malloc(*output_length * sizeof(char));
  if (encoded_data == NULL) return NULL;

  for (int i = 0, j = 0; i < input_length;) {
    uint32_t octet_a = i < input_length ? (unsigned char)input[i++] : 0;
    uint32_t octet_b = i < input_length ? (unsigned char)input[i++] : 0;
    uint32_t octet_c = i < input_length ? (unsigned char)input[i++] : 0;

    uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

    encoded_data[j++] = base64_lookup[(triple >> 3 * 6) & 0x3F];
    encoded_data[j++] = base64_lookup[(triple >> 2 * 6) & 0x3F];
    encoded_data[j++] = base64_lookup[(triple >> 1 * 6) & 0x3F];
    encoded_data[j++] = base64_lookup[(triple >> 0 * 6) & 0x3F];
  }

  for (int i = 0; i < mod_table[input_length % 3]; i++)
    encoded_data[*output_length - 1 - i] = '=';
  
  return encoded_data;
}

char * generate_ws_key(char *ws_client_key, int key_length, int *output_key_length)
{
  printf("key %s\n", ws_client_key);
  char magic_ws_string[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  printf("magic %s\n", magic_ws_string);
  int concatenated_string_length = (int)sizeof(magic_ws_string) + key_length;
  char *concatenated_string = (char *)malloc(concatenated_string_length * sizeof(char));
  memcpy(concatenated_string, ws_client_key, key_length);
  memcpy(concatenated_string + key_length, magic_ws_string, sizeof(magic_ws_string));
  printf("concatenated_key %s\n", concatenated_string);
  unsigned char hash[SHA_DIGEST_LENGTH];
  SHA1((unsigned char *)concatenated_string, concatenated_string_length - 1, hash);
  free(concatenated_string);
  return base_64_encode(hash, SHA_DIGEST_LENGTH, output_key_length);
}

#endif // UTILS_H