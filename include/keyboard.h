#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

void keyboard_init();
void keyboard_routine();
int sys_read_keyboard(char* buffer, int size);

#endif /* __KEYBOARD_H__ */
