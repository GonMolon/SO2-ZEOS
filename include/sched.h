/*
 * sched.h - Estructures i macros pel tractament de processos
 */

#ifndef __SCHED_H__
#define __SCHED_H__

#include <list.h>
#include <types.h>
#include <mm_address.h>

#define NR_TASKS      10
#define KERNEL_STACK_SIZE	1024

enum state_t { ST_RUN, ST_READY, ST_BLOCKED };

struct task_struct {
    int PID;			/* Process ID. This MUST be the first field of the struct. */
    struct list_head anchor;
    int quantum;
    page_table_entry* dir_pages_baseAddr;
    DWord kernel_esp;
};

union task_union {
  struct task_struct task;
  unsigned long stack[KERNEL_STACK_SIZE];    /* pila de sistema, per procés */
};

#define TASK_UNION(task) ((union task_union*) task)

extern union task_union protected_tasks[NR_TASKS+2];
extern union task_union* tasks; /* Vector de tasques */
extern struct task_struct* idle_task;

#define KERNEL_ESP(t) (DWord) &(t)->stack[KERNEL_STACK_SIZE]

#define INITIAL_ESP KERNEL_ESP(&tasks[1])

/* Inicialitza les dades del proces inicial */
void init_task1(void);

void init_idle(void);

void init_sched(void);

struct task_struct* current();

void task_switch(union task_union* t);

void inner_task_switch(union task_union* t);

DWord get_ebp();

void change_context(DWord esp);

struct task_struct* list_head_to_task_struct(struct list_head* l);

int allocate_DIR(struct task_struct* task);

struct task_struct* allocate_process();

page_table_entry* get_PT(struct task_struct* t);

page_table_entry* get_DIR(struct task_struct* t);

void execute_scheduling();

/* Headers for the scheduling policy */
#define QUANTUM 1000

void sched_next_rr();
void update_process_state_rr(struct task_struct* task, struct list_head* dest);
int needs_sched_rr();
void update_sched_data_rr();
void add_process_to_scheduling(struct task_struct* task);

#endif  /* __SCHED_H__ */
