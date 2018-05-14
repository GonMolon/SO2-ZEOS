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
void keyboard_routine() {
    unsigned char value = inb(0x60);
    unsigned char position = value & 0b01111111;
    char action = value >> 7;
    if(action == 0) {
        char key = 'C';
        if(position >= 0 && position < 98) {
            key = char_map[position];
        }
        if(key == '\0') {
            key = 'C';
        }
        printc_xy(0, 0, key);
    }
}
