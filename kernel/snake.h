#ifndef SNAKE_H
#define SNAKE_H

/* The classic game, reachable only through the undocumented `snakeplay`
 * shell command - deliberately absent from `help`. Takes over the screen
 * and the keyboard (raw mode) until you die or press q, then restores
 * both and returns to the shell. */
void snake_run(void);

#endif
