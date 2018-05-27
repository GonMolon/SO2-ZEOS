#include <keyboard.h>
#include <io.h>

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
int remaining;

inline int is_buffer_full() {
    return buff_from - buff_to == 1 || (buff_from == 0 && buff_to == KEYBOARD_BUFFER_SIZE - 1);
}

void keyboard_init() {
    buff_from = 0;
    buff_to = 0;
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
                    if(--remaining == 0 || is_buffer_full()) {
                        
                    }
                }
            }
        }
    }
}

int read_from_keyboard_buffer(char* buffer, int* size) {

}

int sys_read_keyboard(char* buffer, int size) {
    int total_size = size;
    if(list_empty(&keyboardqueue)) {
        size -= read_from_keyboard_buffer(buffer, &size);
    } else {
        update_process_state_rr(current(), &keyboardqueue);
        update_stats(current(), SYS_TO_BLOCKED);
        sched_next_rr();
    }
    while(size > 0) {
        remaining = size;
        update_process_state_rr(current(), &keyboardqueue);
        update_stats(current(), SYS_TO_BLOCKED);
        sched_next_rr();
        size -= read_from_keyboard_buffer(buffer, &size);
    }
    return total_size;
}
