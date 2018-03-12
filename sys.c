/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>
#include <utils.h>
#include <io.h>
#include <mm.h>
#include <mm_address.h>
#include <sched.h>
#include <errno.h>
#include <types.h>
#include <system.h>

#define READ_OPERATION 0
#define WRITE_OPERATION 1

int check_fd(int fd, int permissions) {
    if(fd != 1) return -EBADF; /*EBADF*/
    if(permissions != WRITE_OPERATION) {
        return -EACCES; /*EACCES*/
    }
    return 0;
}

int sys_ni_syscall() {
	return -ENOSYS; /*ENOSYS*/
}

int sys_getpid() {
	return current()->PID;
}

int sys_fork() {
  int PID = -1;

  // creates the child process
  
  return PID;
}

#define CHUNK_SIZE 256

int sys_write(int fd, char* buffer, int size) {
  int error = check_fd(fd, WRITE_OPERATION);
  if(error < 0) {
    return error;
  }
  if(buffer == NULL) {
    return -EBUFFERNULL;
  }
  if(size < 0) {
    return -EINVAL;
  }
  char buffer_copy[CHUNK_SIZE];
  int offset = 0;
  while(error >= 0 && offset < size) {
    int move_size = min(size, CHUNK_SIZE);
    error = copy_from_user(buffer + offset, buffer_copy, move_size);
    offset += move_size;
    sys_write_console(buffer_copy, move_size);
  }
  if(error < 0) {
    return error;
  }
  return size;
}

unsigned long sys_gettime() {
  return zeos_ticks;
}

void sys_exit() {  
}
