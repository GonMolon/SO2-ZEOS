#include <keyboard.h>
#include <io.h>
#include <list.h>
#include <sched.h>
#include <stats.h>

char char_map[] = {
    '\0','\0','1','2','3','4','5','6',
    '7','8','9','0','\'','¡','\0','\0',
    'q','w','e','r','t','y','u','i',
    'o','p','`','+','\0','\0','a','s',
    'd','f','g','h','j','k','l','ñ',
    '\0','º','\0','ç','z','x','c','v',
    'b','n','m',',','.','-','\0','*',
    '\0','\0','\0','\0','\0','\0','\0','\0',
    '\0','\0','\0','\0','\0','\0','\0','7',
    '8','9','-','4','5','6','+','1',
    '2','3','0','\0','\0','\0','<','\0',
    '\0','\0','\0','\0','\0','\0','\0','\0',
    '\0','\0'
};

#define KEYBOARD_BUFFER_SIZE 10
char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
int buff_from;
int buff_to;
struct list_head keyboardqueue;
int remaining_to_read;

inline int is_buffer_full() {
    return buff_from - buff_to == 1 || (buff_from == 0 && buff_to == KEYBOARD_BUFFER_SIZE - 1);
}

void init_keyboard() {
    buff_from = 0;
    buff_to = 0;
    remaining_to_read = 0;
    INIT_LIST_HEAD(&keyboardqueue);
}

void keyboard_routine() {
    unsigned char value = inb(0x60);
    unsigned char position = value & 0b01111111;
    char action = value >> 7;
    if(action == 0) {
        if(position >= 0 && position < 98) {
            char key = char_map[position];
            if(!is_buffer_full()) {
                keyboard_buffer[buff_to++] = key;
                if(buff_to == KEYBOARD_BUFFER_SIZE) {
                    buff_to = 0;
                }
                if(!list_empty(&keyboardqueue)) {
                    if(--remaining_to_read <= 0 || is_buffer_full()) {
                        struct task_struct* task = list_head_to_task_struct(list_first(&keyboardqueue));
                        add_process_to_scheduling(task, BLOCKED_TO_READY, 1);
                        sched_next_rr();  
                    }
                }
            }
        }
    }
}

void block_in_read(int first) {
    update_process_state(current(), &keyboardqueue, first);
    update_stats(current(), SYS_TO_BLOCKED);
    sched_next_rr();
}

int read_from_keyboard_buffer(char* buffer, int size) {
    int count = 0;

    while(size-- > 0 && buff_from != buff_to) {
        buffer[count++] = keyboard_buffer[buff_from++];
        if(buff_from == KEYBOARD_BUFFER_SIZE) {
            buff_from = 0;
        }
    }

    return count;
}

int sys_read_keyboard(char* buffer, int size) {
    int total_size = size;

    if(!list_empty(&keyboardqueue)) { // Another process is already reading
        block_in_read(0);
    }

    size -= read_from_keyboard_buffer(buffer, size);
    while(size > 0) {
        remaining_to_read = size;
        block_in_read(1);
        size -= read_from_keyboard_buffer(&buffer[total_size-size], size);
    }

    return total_size;
}
