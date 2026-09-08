#ifndef KEYBOARD_H
#define KEYBOARD_H

/* Keys that have no ASCII value are reported as codes above 0xFF, so a
 * caller can switch on the return value of keyboard_getch() directly. */
#define KEY_UP    0x101
#define KEY_DOWN  0x102
#define KEY_LEFT  0x103
#define KEY_RIGHT 0x104

void keyboard_install(void);

/* Waits for a keypress and returns it: an ASCII character, or one of the
 * KEY_* codes above. Sleeps the CPU while waiting rather than spinning.
 *
 * Note the driver does NOT echo: the interrupt handler only queues keys,
 * and everything else (echo, backspace, history, completion) happens in
 * normal context - see readline.c. Keeping it out of the ISR is what
 * makes those features possible at all, since they need the shell's
 * command table and the filesystem. */
int keyboard_getch(void);

/* Same, but never waits: returns 0 when nothing has been typed. */
int keyboard_poll(void);

#endif
