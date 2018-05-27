#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

void init_keyboard();
void keyboard_routine();
int sys_read_keyboard(char* buffer, int size);

#endif /* __KEYBOARD_H__ */
