/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <sched.h>
#include <mm.h>
#include <io.h>
#include <list.h>

/**
 * Container for the Task array and 2 additional pages (the first and the last one)
 * to protect against out of bound accesses.
 */
union task_union protected_tasks[NR_TASKS+2] __attribute__((__section__(".data.task")));

union task_union* tasks = &protected_tasks[1]; /* == union task_union task[NR_TASKS] */

struct task_struct* idle_task;

struct list_head free_queue;
struct list_head readyqueue;

int last_PID;

struct task_struct* list_head_to_task_struct(struct list_head* l) {
    return list_entry(l, struct task_struct, anchor);
}

struct semaphore semaphores[NR_SEMAPHORES];

/* get_DIR - Returns the Page Directory address for task 't' */
page_table_entry* get_DIR(struct task_struct* t) {
	return t->dir_pages_baseAddr;
}

/* get_PT - Returns the Page Table address for task 't' */
page_table_entry* get_PT(struct task_struct* t) {
	return (page_table_entry*)(((unsigned int)(t->dir_pages_baseAddr->bits.pbase_addr)) << 12);
}

struct task_struct* allocate_process(page_table_entry* dir) {
    if(list_empty(&free_queue)) {
        return NULL;
    }
    struct task_struct* task = list_head_to_task_struct(list_first(&free_queue));
    if(dir == NULL) {
        if(allocate_DIR(task) == 0) {
            return NULL;
        }
    } else {
        reuse_DIR(dir, task);
    }
    list_del(&task->anchor);

    task->PID = last_PID++;
    task->heap_top = (void*) HEAP_START + 1;
    INIT_LIST_HEAD(&task->semaphores);
    task->sem_deleted = 0;
    task->quantum = DEFAULT_QUANTUM;
    reset_stats(task);
    return task;
}

void free_semaphores(struct task_struct* task);

void free_process(struct task_struct* task) {
    update_process_state_rr(task, &free_queue); // We free its PCB adding it into the free queue
    free_DIR(task);
    free_semaphores(task);
}

void cpu_idle(void) {
	__asm__ __volatile__("sti": : :"memory");
    while(1);
}

void update_TSS(struct task_struct* task) {
    tss.esp0 = KERNEL_ESP(TASK_UNION(task));
}

void inner_task_switch(union task_union* t) {
    current()->kernel_esp = get_ebp();

    struct task_struct* task = &t->task;
    update_TSS(task);
    if(current()->dir_pages_baseAddr != task->dir_pages_baseAddr) {
        set_cr3(get_DIR(task));
    }
    change_context(task->kernel_esp);
}

void init_idle(void) {
    idle_task = allocate_process(NULL);

    union task_union* task_u = TASK_UNION(idle_task);
    task_u->stack[KERNEL_STACK_SIZE - 1] = (DWord) &cpu_idle;
    idle_task->kernel_esp = (DWord) &task_u->stack[KERNEL_STACK_SIZE - 2];

    idle_task->state = ST_READY;
}

void init_task1(void) {
    struct task_struct* task1 = allocate_process(NULL);

    set_user_pages(task1);

    update_TSS(task1);  // Updating TSS
    set_cr3(get_DIR(task1)); // Updating current page directory

    task1->state = ST_RUN; // Since it's going to start running
    update_stats(task1, FREE_TO_READY);
}

void init_sched() {
    current_ticks = 0;
    last_PID = 0;

    INIT_LIST_HEAD(&free_queue);
    INIT_LIST_HEAD(&readyqueue);
    for(int i = 0; i < NR_TASKS; ++i) {
        update_process_state_rr(&tasks[i].task, &free_queue);
    }

    for(int i = 0; i < NR_SEMAPHORES; ++i) {
        semaphores[i].used = 0;
        semaphores[i].count = 0;
    }
}

struct task_struct* current() {
    int ret_value;

    __asm__ __volatile__(
    	"movl %%esp, %0"
    : "=g" (ret_value)
    );
    return (struct task_struct*)(ret_value & 0xfffff000);
}



// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// SCHEDULING POLICY
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

int current_ticks;

void execute_scheduling() {
    update_sched_data_rr();
    if(needs_sched_rr()) {
        if(current() != idle_task) {
            update_process_state_rr(current(), &readyqueue);
        }
        update_stats(current(), SYS_TO_READY);
        sched_next_rr();
    }
}

void update_sched_data_rr() {
    current_ticks++;
    update_stats(current(), CURRENT_TICKS_UPDATED); // To refresh the remaing ticks field
}

int needs_sched_rr() {
    return 
        (current() == idle_task && !list_empty(&readyqueue)) // There is another process waiting and we are in idle
        || current()->st.remaining_ticks == 0; // Or current task has finished its quantum
}

void add_process_to_scheduling(struct task_struct* task, enum event e, int is_first) {
    update_process_state(task, &readyqueue, is_first);
    update_stats(task, e);
}

void update_process_state_rr(struct task_struct* task, struct list_head* dest) {
    update_process_state(task, dest, 0);
}

void update_process_state(struct task_struct* task, struct list_head* dest, int is_first) {
    if(task->anchor.next != NULL && task->anchor.prev != NULL) {
        list_del(&task->anchor);
    }
    if(dest == NULL) {
        task->state = ST_RUN;
    } else {
        if(is_first) {
            list_add(&task->anchor, dest);
        } else {
            list_add_tail(&task->anchor, dest);
        }
        if(dest == &readyqueue) {
            task->state = ST_READY;
        } else if(dest == &free_queue) {
            task->state = ST_INVALID;
        } else {
            task->state = ST_BLOCKED;
        }
    }
}

void sched_next_rr() {
    struct task_struct* next_task;
    if(!list_empty(&readyqueue)) { // There is another task waiting
        next_task = list_head_to_task_struct(list_first(&readyqueue));
    } else { // There is no other task in ready so we activate idle_task
        next_task = idle_task;
    }
    update_process_state_rr(next_task, NULL);

    update_stats(next_task, READY_TO_SYS);
    current_ticks = 0;
    update_stats(next_task, CURRENT_TICKS_UPDATED);
    task_switch(TASK_UNION(next_task));
}

int get_quantum(struct task_struct* task) {
    return task->quantum;
}

void set_quantum(struct task_struct* task, int new_quantum) {
    task->quantum = new_quantum;
    update_stats(current(), QUANTUM_UPDATED);
}



