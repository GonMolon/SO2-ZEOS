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

extern struct list_head blocked;


/* get_DIR - Returns the Page Directory address for task 't' */
page_table_entry* get_DIR(struct task_struct* t) {
	return t->dir_pages_baseAddr;
}

/* get_PT - Returns the Page Table address for task 't' */
page_table_entry* get_PT(struct task_struct* t) {
	return (page_table_entry*)(((unsigned int)(t->dir_pages_baseAddr->bits.pbase_addr)) << 12);
}


int allocate_DIR(struct task_struct* t) {
	int pos;

	pos = ((int)t-(int)tasks)/sizeof(union task_union);

	t->dir_pages_baseAddr = (page_table_entry*) &dir_pages[pos];

	return 1;
}

struct task_struct* allocate_process() {
    if(list_empty(&free_queue)) {
        return NULL;
    }
    struct task_struct* task = list_head_to_task_struct(list_first(&free_queue));
    list_del(&task->anchor);

    task->PID = last_PID++;
    reset_stats(task);
    update_stats(task, PROCESS_CREATED);
    return task;
}

void free_process_resources(struct task_struct* task) {
    update_process_state_rr(task, &free_queue); // We free its PCB adding it into the free queue
    free_user_pages(task);  // We free the phisical memory used by this process (only data). 
                            // Ideally it should also free code frames if it were the last process in executiom
                            // Note that apart from freeing the physical memory, it's also necessary to reset the 
                            // page table, because if there's another future process that uses it, it would have access
                            // to those physical frames. (That is done inside free_user_pages()). It would also be
                            // important to "delete" their PT because if we don't do it, the advantages from the directory
                            // scheme wouldn't exist anymore. In this particular case of ZEOS is not necessary.
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
    set_cr3(get_DIR(task));
    change_context(task->kernel_esp);
}

void init_idle(void) {
    idle_task = allocate_process();
    allocate_DIR(idle_task);

    union task_union* task_u = TASK_UNION(idle_task);
    task_u->stack[KERNEL_STACK_SIZE - 1] = (DWord) &cpu_idle;
    idle_task->kernel_esp = (DWord) &task_u->stack[KERNEL_STACK_SIZE - 2];

    idle_task->state = ST_READY;
}

void init_task1(void) {
    struct task_struct* task1 = allocate_process();
    set_quantum(task1, DEFAULT_QUANTUM);
    allocate_DIR(task1); // Will assign to it a page directory. In fact, it will be the i'th page directory if this task is the i'th one
    set_user_pages(task1);

    update_TSS(task1);  // Updating TSS
    set_cr3(get_DIR(task1)); // Updating current page directory

    task1->state = ST_RUN; // Since it's going to start running
}

void init_sched() {
    current_ticks = 0;
    last_PID = 0;

    INIT_LIST_HEAD(&free_queue);
    INIT_LIST_HEAD(&readyqueue);
    for(int i = 0; i < NR_TASKS; ++i) {
        update_process_state_rr(&tasks[i].task, &free_queue);
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

void add_process_to_scheduling(struct task_struct* task) {
    // Reseting stats of process
    reset_stats(task);
    update_stats(task, PROCESS_CREATED);
    
    update_process_state_rr(task, &readyqueue);
}

void update_process_state_rr(struct task_struct* task, struct list_head* dest) {
    if(dest == NULL) {
        list_del(&task->anchor);
        task->state = ST_RUN;
    } else {
        list_add_tail(&task->anchor, dest);
        if(dest == &readyqueue) {
            task->state = ST_READY;
        } else if(dest == &free_queue) {
            task->state = ST_INVALID;
        }
    }
}

void sched_next_rr() {
    struct task_struct* next_task;
    if(!list_empty(&readyqueue)) { // There is another task waiting
        next_task = list_head_to_task_struct(list_first(&readyqueue));
        update_process_state_rr(next_task, NULL);
    } else { // There is no other task in ready so we activate idle_task
        next_task = idle_task;
    }
    update_stats(current(), SYS_TO_READY);
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



