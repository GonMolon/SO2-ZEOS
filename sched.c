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
struct list_head ready_queue;

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
    struct list_head* free_task = list_first(&free_queue);
    if(free_task == &free_queue) {
        return NULL;
    }
    struct task_struct* task = list_head_to_task_struct(free_task);
    list_del(free_task);
    task->PID = last_PID++;
    return task;
}

void add_process_to_scheduling(struct task_struct* task) {
    list_add_tail(&task->anchor, &ready_queue);
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
}

void init_task1(void) {
    struct task_struct* task1 = allocate_process();
    allocate_DIR(task1); // Will assign to it a page directory. In fact, it will be the i'th page directory if this task is the i'th one
    set_user_pages(task1);

    update_TSS(task1);  // Updating TSS
    set_cr3(get_DIR(task1)); // Updating current page directory
}

void init_sched() {
    last_PID = 0;

    INIT_LIST_HEAD(&free_queue);
    INIT_LIST_HEAD(&ready_queue);
    for(int i = 1; i < NR_TASKS + 1; ++i) {
        list_add(&protected_tasks[i].task.anchor, &free_queue);
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

