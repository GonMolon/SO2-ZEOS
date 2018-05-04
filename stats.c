#include <stats.h>
#include <sched.h>
#include <utils.h>

void reset_stats(struct task_struct* task) {
    task->st.user_ticks = 0;
    task->st.system_ticks = 0;
    task->st.blocked_ticks = 0;
    task->st.ready_ticks = 0;
    task->st.elapsed_total_ticks = 0;
    task->st.total_trans = 0;
    task->st.remaining_ticks = 0;
}

void update_stats(struct task_struct* task, enum event e) {
    if(e == QUANTUM_UPDATED || e == CURRENT_TICKS_UPDATED) {
        int aux = task->quantum - current_ticks;
        task->st.remaining_ticks = aux >= 0 ? aux : 0;
    } else {
        unsigned long elapsed = task->st.elapsed_total_ticks;
        unsigned long current = get_ticks();
        unsigned long incr = current - elapsed;
        if(e == TO_SCHEDULING) {
            task->st.remaining_ticks = task->quantum;
        } else if(e == USER_TO_SYS) {
            task->st.user_ticks += incr;
        } else if(e == SYS_TO_USER) {
            task->st.system_ticks += incr;
        } else if(e == SYS_TO_READY) {
            task->st.system_ticks += incr;
            task->st.remaining_ticks = task->quantum;
        } else if(e == READY_TO_SYS) {
            task->st.ready_ticks += incr;
            ++task->st.total_trans;
        } else if(e == USER_TO_BLOCKED) {

        } else if(e == BLOCKED_TO_READY) {

        } else if(e == REFRESH) {
            if(task->state == ST_READY) {
                task->st.ready_ticks += incr;
            } else if(task->state == ST_BLOCKED) {
                task->st.blocked_ticks += incr;
            } else {
                while(1); // ERROR
            }
        }
        task->st.elapsed_total_ticks = current;
    }
}

void update_entering_sys() {
    update_stats(current(), USER_TO_SYS);
}

void update_leaving_sys() {
    update_stats(current(), SYS_TO_USER);
}