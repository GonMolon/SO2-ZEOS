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
#include <keyboard.h>

#define READ_OPERATION 0
#define WRITE_OPERATION 1

int check_fd(int fd, int permissions) {
    if(fd != 0 && fd != 1) {
        return -EBADF; /*EBADF*/
    }
    if(fd == 1 && permissions != WRITE_OPERATION) {
        return -EBADF;
    }
    if(fd == 0 && permissions != READ_OPERATION) {
        return -EBADF;
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

    // Inherit father's heap
    task->heap_top = current()->heap_top;

    // Copying system stack into child (Only SAVE_ALL, since we don't need the entire stack!)
    int stack_pos = copy_process_stack(task);
    
    // Setting child system context to be ready for whenever it gets activated by a task_switch
    TASK_UNION(task)->stack[stack_pos] = (DWord) &ret_from_fork;
    task->kernel_esp = (DWord) &TASK_UNION(task)->stack[stack_pos - 1];

    int NUM_PAG_HEAP = get_heap_page_size(current()->heap_top);
    // Finding free frames to store (data+stack)+heap
    int data_frames[NUM_PAG_DATA + NUM_PAG_HEAP];
    for(int i = 0; i < NUM_PAG_DATA + NUM_PAG_HEAP; ++i) {
        int new_frame = alloc_frame();
        if(new_frame == -1) {
            // Free-ing reserved frames since we don't have enought for the child
            for(int j = 0; j < i; ++j) {
                free_frame(data_frames[j]);
            }
            return -ENOMEM;
        }
        data_frames[i] = new_frame;
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%
    // CODE
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%

    // Copying code pages (the code pages point to the same frames as the father)
    for(int i = 0; i < NUM_PAG_CODE; ++i) {
        get_PT(task)[PAG_LOG_INIT_CODE + i] = get_PT(current())[PAG_LOG_INIT_CODE + i];
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%
    // DATA
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%

    for(int i = 0; i < NUM_PAG_DATA; ++i) {
        int from_page = PAG_LOG_INIT_DATA + i;
        int to_page = PAG_LOG_INIT_HEAP + 1 + i;
        // Data pages point to new frames
        set_ss_pag(get_PT(task), from_page, data_frames[i]);
        // Augmenting logical address space of father temporarly to copy data
        set_ss_pag(get_PT(current()), to_page, data_frames[i]);
        // Copying data
        copy_data((void*) (from_page*PAGE_SIZE), (void*) (to_page*PAGE_SIZE), PAGE_SIZE);
        // Reverting previous step after having copied data
        del_ss_pag(get_PT(current()), to_page);
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%
    // HEAP
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%

    for(int i = 0; i < NUM_PAG_HEAP; ++i) {
        int from_page = PAG_LOG_INIT_HEAP - i;
        int to_page = PAG_LOG_INIT_HEAP + 1 + NUM_PAG_DATA + i;
        set_ss_pag(get_PT(task), from_page, data_frames[NUM_PAG_DATA + i]);
        set_ss_pag(get_PT(current()), to_page, data_frames[NUM_PAG_DATA + i]);
        copy_data((void*) (from_page*PAGE_SIZE), (void*) (to_page*PAGE_SIZE), PAGE_SIZE);
        del_ss_pag(get_PT(current()), to_page);
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%

    // Flush TLB
    set_cr3(get_DIR(current()));

    // Adding process to ready_queue
    add_process_to_scheduling(task, FREE_TO_READY, 0);

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
    *((DWord*) (stack - 4)) = (DWord) exit_func; // exit function address
    TASK_UNION(task)->stack[KERNEL_STACK_SIZE - 2] = (DWord) (stack - 4);
    TASK_UNION(task)->stack[KERNEL_STACK_SIZE - 5] = (DWord) function;

    add_process_to_scheduling(task, FREE_TO_READY, 0);
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
        return error;
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

int sys_read(int fd, char* buffer, int size) {
    int error = check_fd(fd, READ_OPERATION);
    if(error < 0) {
        return error;
    }
    if(buffer == NULL) {
        return -EFAULT;
    }
    if(size < 0) {
        return -EINVAL;
    }
    if(!access_ok(VERIFY_WRITE, buffer, size)) {
        return -EFAULT;
    }
    return sys_read_keyboard(buffer, size);
}

unsigned long sys_gettime() {
    return zeos_ticks;
}

int sys_sem_init(int n_sem, unsigned int value) {
    if(n_sem < 0 || n_sem >= NR_SEMAPHORES) {
        return -EINVAL;
    }

    if(value < 0) {
        return -EINVAL;
    }

    struct semaphore* sem = &semaphores[n_sem];

    if(sem->used) {
        return -EBUSY;
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
        return -EINVAL;
    }

    struct semaphore* sem = &semaphores[n_sem];

    if(!sem->used) {
        return -EINVAL;
    }

    if(sem->count == 0) {
        update_process_state_rr(current(), &sem->blocked);
        update_stats(current(), SYS_TO_BLOCKED);
        sched_next_rr();
        if(current()->sem_deleted) { // Check if the semaphore was removed while blocked
            current()->sem_deleted = 0;
            return -1;
        }
    } else {
        sem->count--;
    }

    return 0;
}

int sys_sem_signal(int n_sem) {
    if(n_sem < 0 || n_sem >= NR_SEMAPHORES) {
        return -EINVAL;
    }

    struct semaphore* sem = &semaphores[n_sem];

    if(!sem->used) {
        return -EINVAL;
    }

    if(list_empty(&sem->blocked)) {
        ++sem->count;
    } else {
        struct task_struct* task = list_head_to_task_struct(list_first(&sem->blocked));
        add_process_to_scheduling(task, BLOCKED_TO_READY, 0);
    }
    return 0;
}

int sys_sem_destroy(int n_sem) {
    if(n_sem < 0 || n_sem >= NR_SEMAPHORES) {
        return -EINVAL;
    }

    struct semaphore* sem = &semaphores[n_sem];

    if(!sem->used) {
        return -EINVAL;
    }

    if(current()->PID != sem->owner->PID) {
        return -1;
    }
    
    sem->used = 0;
    list_del(&sem->anchor);

    list_for_each_safe(task_anchor, &sem->blocked) {
        struct task_struct* task = list_head_to_task_struct(task_anchor);
        add_process_to_scheduling(task, BLOCKED_TO_READY, 0);
        task->sem_deleted = 1;
    }

    return 0;
}

void free_semaphores(struct task_struct* task) {
    list_for_each_safe(sem_anchor, &task->semaphores) {
        struct semaphore* sem = list_entry(sem_anchor, struct semaphore, anchor);
        int n_sem = sem - &semaphores[0];
        sys_sem_destroy(n_sem);
    }
    INIT_LIST_HEAD(&task->semaphores);
}

void* sys_sbrk(int increment) {
    if(increment == 0) {
        return current()->heap_top;
    }

    void* new_heap_top = (void*) (((int) current()->heap_top) - increment);
    if((unsigned int) new_heap_top > HEAP_START) {
        if(increment < 0) {
            new_heap_top = (void*) HEAP_START;
        } else {
            return (void*) -ENOMEM; // Overflow
        }
    }
    int HEAP_PAGES_ACT = get_heap_page_size(current()->heap_top);
    int HEAP_PAGES_NEW = get_heap_page_size(new_heap_top);

    void* result;
    if(HEAP_PAGES_ACT < HEAP_PAGES_NEW) { // Heap is incremented
        int PAGES_INCR = HEAP_PAGES_NEW - HEAP_PAGES_ACT;

        // Check that we don't enter in another memory region
        for(int i = 0; i < PAGES_INCR; ++i) {
            if(!is_ss_pag_free(get_PT(current()), PAG_LOG_INIT_HEAP - HEAP_PAGES_ACT - i)) {
                return (void*) -ENOMEM;
            }
        }

        // Reserve new frames for the new heap pages
        int frames[PAGES_INCR];
        for(int i = 0; i < PAGES_INCR; ++i) {
            int new_frame = alloc_frame();
            if(new_frame == -1) {
                for(int j = 0; j < i; ++j) {
                    free_frame(frames[j]);
                }
                return (void*) -ENOMEM;
            }
            frames[i] = new_frame;
        }

        // Assign the new frames
        for(int i = 0; i < PAGES_INCR; ++i) {
            set_ss_pag(get_PT(current()), PAG_LOG_INIT_HEAP - HEAP_PAGES_ACT - i, frames[i]);
        }

        result = new_heap_top + 1; // Since we need to return the first writeable byte

    } else { // Heap is decremented
        int PAGES_INCR = HEAP_PAGES_ACT - HEAP_PAGES_NEW;
        
        // Free the removed pages and its respective frames
        for(int i = 0; i < PAGES_INCR; ++i) {
            int page = PAG_LOG_INIT_HEAP - HEAP_PAGES_NEW - i;
            free_frame(get_frame(get_PT(current()), page));
            del_ss_pag(get_PT(current()), page);
        }
        set_cr3(get_DIR(current()));
        result = new_heap_top;
    }

    current()->heap_top = new_heap_top;
    return result;
}

