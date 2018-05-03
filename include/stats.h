#ifndef STATS_H
#define STATS_H

/* Structure used by 'get_stats' function */
struct stats {
    unsigned long user_ticks;
    unsigned long system_ticks;
    unsigned long blocked_ticks;
    unsigned long ready_ticks;
    unsigned long elapsed_total_ticks;
    unsigned long total_trans; /* Number of times the process has got the CPU: READY->RUN transitions */
    unsigned long remaining_ticks;
};

enum event {
    PROCESS_CREATED,
    USER_TO_SYS,
    SYS_TO_USER,
    SYS_TO_READY,
    READY_TO_SYS,
    USER_TO_BLOCKED,
    BLOCKED_TO_READY,
    QUANTUM_UPDATED,
    CURRENT_TICKS_UPDATED,
    REFRESH
};

struct task_struct;

void reset_stats(struct task_struct* task);

void update_stats(struct task_struct* task, enum event e);

#endif /* !STATS_H */
