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

void sys_exit() {

    free_process_resources(current());

    current()->quantum = -1; // So the scheduler knows it doesn't have to be added in ready again
    sched_next_rr();
}

int ret_from_fork() {
    return 0;
}

int sys_fork() {

    struct task_struct* task = allocate_process();
    if(task == NULL) {
        return -NOT_FREE_TASK;
    }
    int PID = task->PID;

    // Copying system context from father to child
    *TASK_UNION(task) = *TASK_UNION(current());

    // Setting child PID
    task->PID = PID;

    // Setting child system context to be ready for whenever it gets activated by a task_switch
    DWord* father_ebp = (DWord*) get_ebp();
    int stack_pos = (((DWord) father_ebp) & (PAGE_SIZE - 1))/4;
    TASK_UNION(task)->stack[stack_pos] = (DWord) &ret_from_fork;
    task->kernel_esp = (DWord) &TASK_UNION(task)->stack[stack_pos-1];

    // Assigning a free page directory to child
    allocate_DIR(task);

    // Finding free frames to store data+stack
    int data_frames[NUM_PAG_DATA];
    for(int i = 0; i < NUM_PAG_DATA; ++i) {
        int new_frame = alloc_frame();
        if(new_frame == -1) {
            // Free-ing reserved frames since we don't have enought for the child
            for(int j = 0; j < i; ++j) {
                free_frame(j);
            }
            // TODO free frames
            return -NOT_FREE_FRAMES;
        }
        data_frames[i] = new_frame;
    }

    // Copying code pages (the code pages point to the same frames as the father)
    for(int i = 0; i < NUM_PAG_CODE; ++i) {
        get_PT(task)[PAG_LOG_INIT_CODE + i] = get_PT(current())[PAG_LOG_INIT_CODE + i];
    }

    // Data pages point to new frames
    for(int i = 0; i < NUM_PAG_DATA; ++i) {
        set_ss_pag(get_PT(task), PAG_LOG_INIT_DATA + i, data_frames[i]);
    }

    // Augmenting logical address space of father temporarly to copy data
    for(int i = 0; i < NUM_PAG_DATA; ++i) {
        set_ss_pag(get_PT(current()), PAG_LOG_INIT_DATA + NUM_PAG_DATA + i, data_frames[i]);
    }

    // Copying data
    copy_data((void*) L_USER_START + NUM_PAG_CODE*PAGE_SIZE, (void*) L_USER_START + (NUM_PAG_CODE+NUM_PAG_DATA)*PAGE_SIZE, PAGE_SIZE*NUM_PAG_DATA);

    // Reverting previous step after having copied data
    for(int i = 0; i < NUM_PAG_DATA; ++i) {
        del_ss_pag(get_PT(current()), PAG_LOG_INIT_DATA + NUM_PAG_DATA + i);
    }

    // Flush TLB
    set_cr3(get_DIR(current()));

    // Adding process to ready_queue
    add_process_to_scheduling(task);

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
