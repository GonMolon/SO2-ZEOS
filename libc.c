/*
 * libc.c 
 */

#include <libc.h>

#include <types.h>

int errno = 0;

void perror() {
  char* error_message;
  switch(errno) {
    case ENOSYS:
      error_message = "System call not defined";
      break;
    case EBADF:
      error_message = "Bad file descriptor identifier";
      break;
    case EACCES:
      error_message = "Wrong permission writing to file";
      break;
    case EBUFFERNULL:
      error_message = "Buffer addres is null";
      break;
    case EBUFFERSIZE:
      error_message = "Buffer size is equal or less than 0";
      break;
  }
  write(1, error_message, strlen(error_message));
}

void itoa(int a, char* b) {
  int i, i1;
  char c;
  
  if(a == 0) {
    b[0] = '0'; 
    b[1] = 0; 
    return;
  }
  
  i = 0;
  while(a > 0) {
    b[i] = (a%10) + '0';
    a = a/10;
    i++;
  }
  
  for(i1 = 0; i1 < i/2; i1++) {
    c = b[i1];
    b[i1] = b[i-i1-1];
    b[i-i1-1] = c;
  }
  b[i] = 0;
}

int strlen(char* a) {
  int i;
  
  i = 0;
  
  while(a[i] != 0) i++;
  
  return i;
}

