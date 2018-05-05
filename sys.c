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
#include <stats.h>

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
    free_process(current());
    sched_next_rr();
}

int ret_from_fork() {
    return 0;
}

// It's important to be "inline" because get_ebp needs to return the ebp of the sys_clone
inline int copy_process_stack(struct task_struct* task) {
    void* father_ebp = (void*) get_ebp();
    int stack_pos = (((DWord) father_ebp) & (PAGE_SIZE - 1))/4;

    void* from = &TASK_UNION(current())->stack[stack_pos + 1];
    void* to = &TASK_UNION(task)->stack[stack_pos + 1];
    int size = (DWord) (((void*) &TASK_UNION(current())->stack[KERNEL_STACK_SIZE]) - from);
    copy_data(from, to, size);

    return stack_pos;
}

int sys_fork() {

    struct task_struct* task = allocate_process(NULL);
    if(task == NULL) {
        return -EAGAIN;
    }
    // Inherit father's quantum
    task->quantum = current()->quantum;

    // Copying system stack into child (Only SAVE_ALL, since we don't need the entire stack!)
    int stack_pos = copy_process_stack(task);
    
    // Setting child system context to be ready for whenever it gets activated by a task_switch
    TASK_UNION(task)->stack[stack_pos] = (DWord) &ret_from_fork;
    task->kernel_esp = (DWord) &TASK_UNION(task)->stack[stack_pos - 1];

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
            return -ENOMEM;
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
    add_process_to_scheduling(task, FREE_TO_READY);

    return task->PID;
}

int sys_clone(void (*function)(void), void* stack, void* exit_func) {
    if(!access_ok(VERIFY_WRITE, function, 4) || !access_ok(VERIFY_WRITE, stack, 4)) {
        return -EFAULT;
    }

    struct task_struct* task = allocate_process(current()->dir_pages_baseAddr);
    if(task == NULL) {
        return -EAGAIN;
    }
    task->quantum = current()->quantum;

    // Setting child system context to be ready for whenever it gets activated by a task_switch
    int stack_pos = copy_process_stack(task);
    task->kernel_esp = (DWord) &TASK_UNION(task)->stack[stack_pos];
    *((DWord*) (stack + 4)) = (DWord) exit_func; // exit function address
    TASK_UNION(task)->stack[KERNEL_STACK_SIZE - 2] = (DWord) (stack + 4);
    TASK_UNION(task)->stack[KERNEL_STACK_SIZE - 5] = (DWord) function;

    add_process_to_scheduling(task, FREE_TO_READY);
    return task->PID;
}

int sys_get_stats(int pid, struct stats* st) {

    if(pid < 0) {
        return -EINVAL;
    }

    if(st == NULL || !access_ok(VERIFY_WRITE, st, sizeof(struct stats))) {
        return -EFAULT;
    }

    for(int i = 0; i < NR_TASKS; ++i) {
        if(tasks[i].task.PID == pid) {
            if(tasks[i].task.state == ST_INVALID) {
                return -ESRCH;
            }
            enum state_t state = tasks[i].task.state;
            if(state == ST_READY || state == ST_BLOCKED) {
               update_stats(&tasks[i].task, REFRESH); 
            }
            *st = tasks[i].task.st;
            return 0;
        }
    }
    return -ESRCH;
}

#define CHUNK_SIZE 256

int sys_write(int fd, char* buffer, int size) {
    int error = check_fd(fd, WRITE_OPERATION);
    if(error < 0) {
        return -error;
    }
    if(buffer == NULL) {
        return -EFAULT;
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

int sys_sem_init(int n_sem, unsigned int value) {
    if(n_sem < 0 || n_sem >= NR_SEMAPHORES) {
        return -1;
    }

    if(value < 0) {
        return -1;
    }

    struct semaphore* sem = &semaphores[n_sem];

    if(sem->used) {
        return -1;
    }
    sem->used = 1;
    sem->count = value;
    sem->owner = current();
    INIT_LIST_HEAD(&sem->blocked);
    list_add(&sem->anchor, &current()->semaphores);
    return 0;
}

int sys_sem_wait(int n_sem) {
    if(n_sem < 0 || n_sem >= NR_SEMAPHORES) {
        return -1;
    }

    struct semaphore* sem = &semaphores[n_sem];

    if(!sem->used) {
        return -1;
    }

    if(sem->count == 0) {
        int owner_PID = sem->owner->PID;
        update_process_state_rr(current(), &sem->blocked);
        update_stats(current(), SYS_TO_BLOCKED);
        sched_next_rr();
        if(owner_PID != sem->owner->PID || sem->owner->state == ST_INVALID) { // Check if the semaphore was removed while blocked
            return -1;
        }
    } else {
        sem->count--;
    }

    return 0;
}

int sys_sem_signal(int n_sem) {
    if(n_sem < 0 || n_sem >= NR_SEMAPHORES) {
        return -1;
    }

    struct semaphore* sem = &semaphores[n_sem];

    if(!sem->used) {
        return -1;
    }

    if(list_empty(&sem->blocked)) {
        ++sem->count;
    } else {
        add_process_to_scheduling(list_head_to_task_struct(&sem->blocked), BLOCKED_TO_READY);
    }
    return 0;
}

int sys_sem_destroy(int n_sem) {
    if(n_sem < 0 || n_sem >= NR_SEMAPHORES) {
        return -1;
    }

    struct semaphore* sem = &semaphores[n_sem];

    if(current()->PID != sem->owner->PID) {
        return -1;
    }
    
    if(!sem->used) {
        return -1;
    }
    sem->used = 0;

    list_for_each_safe(task_anchor, &sem->blocked) {
        add_process_to_scheduling(list_head_to_task_struct(task_anchor), BLOCKED_TO_READY);
    }

    return 0;
}

void free_semaphores(struct task_struct* task) {
    list_for_each(sem_anchor, &task->semaphores) {
        struct semaphore* sem = list_entry(sem_anchor, struct semaphore, anchor);
        int n_sem = sem - &semaphores[0];
        sys_sem_destroy(n_sem);
    }
}

