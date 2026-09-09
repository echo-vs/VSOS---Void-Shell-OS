/* snake.c — the classic game, hidden behind `snakeplay`.
 *
 * Two things it needs that the rest of the kernel didn't: keys without
 * waiting for Enter (keyboard raw mode) and drawing at an arbitrary
 * screen cell (print_char_at). Both live in their own drivers rather
 * than here.
 *
 * There is no timer driver yet, so the tick is a busy-wait loop. Its
 * length is therefore in "however fast this machine is" units, not
 * milliseconds - which is why [ and ] adjust it in-game.
 */
#include "snake.h"
#include "screen.h"
#include "keyboard.h"
#include "string.h"

#define FIELD_W 60
#define FIELD_H 18
#define ORIGIN_X 9              /* screen column of the left border */
#define ORIGIN_Y 3              /* screen row of the top border */
#define MAX_LEN 200

/* Tick length in busy-loop iterations, not milliseconds - there's no
 * timer driver yet. Deliberately slow to start; ] speeds it up. */
#define SPEED_START 16000000u
#define SPEED_MIN    2000000u
#define SPEED_MAX   40000000u
#define SPEED_STEP   2000000u

/* colors */
#define C_BORDER 0x0b
#define C_HEAD   0x0a
#define C_BODY   0x02
#define C_FOOD   0x0c
#define C_TEXT   0x0f
#define C_DIM    0x08

static unsigned char sx[MAX_LEN];   /* body cells, index 0 is the head */
static unsigned char sy[MAX_LEN];
static int slen;
static int dir;                     /* 0 up, 1 right, 2 down, 3 left */
static unsigned char food_x, food_y;
static unsigned int score;
static unsigned int rng;
static unsigned int speed;

static unsigned int rnd(void) {
    /* xorshift32; rng is seeded non-zero, and this can never reach zero */
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static void draw_cell(int x, int y, char c, unsigned char color) {
    print_char_at(ORIGIN_Y + 1 + y, ORIGIN_X + 1 + x, c, color);
}

static void put_str(int row, int col, const char *s, unsigned char color) {
    for (int i = 0; s[i]; i++) print_char_at(row, col + i, s[i], color);
}

static void draw_border(void) {
    for (int x = 0; x < FIELD_W + 2; x++) {
        print_char_at(ORIGIN_Y, ORIGIN_X + x, '-', C_BORDER);
        print_char_at(ORIGIN_Y + FIELD_H + 1, ORIGIN_X + x, '-', C_BORDER);
    }
    for (int y = 1; y <= FIELD_H; y++) {
        print_char_at(ORIGIN_Y + y, ORIGIN_X, '|', C_BORDER);
        print_char_at(ORIGIN_Y + y, ORIGIN_X + FIELD_W + 1, '|', C_BORDER);
    }
}

static void draw_score(void) {
    char buf[12];
    vs_itoa(score, buf);
    put_str(ORIGIN_Y - 1, ORIGIN_X, "score: ", C_TEXT);
    put_str(ORIGIN_Y - 1, ORIGIN_X + 7, buf, C_HEAD);
    put_str(ORIGIN_Y - 1, ORIGIN_X + 7 + (int) vs_strlen(buf), "   ", C_TEXT);
}

static void place_food(void) {
    while (1) {
        unsigned char fx = (unsigned char) (rnd() % FIELD_W);
        unsigned char fy = (unsigned char) (rnd() % FIELD_H);

        int clash = 0;
        for (int i = 0; i < slen; i++) {
            if (sx[i] == fx && sy[i] == fy) { clash = 1; break; }
        }
        if (clash) continue;   /* the field is far larger than MAX_LEN */

        food_x = fx;
        food_y = fy;
        draw_cell(fx, fy, '*', C_FOOD);
        return;
    }
}

static void tick_delay(void) {
    /* volatile so the whole loop isn't optimized away at -Os */
    for (volatile unsigned int i = 0; i < speed; i++) { }
}

void snake_run(void) {
    clear_screen();
    put_str(1, ORIGIN_X, "S N A K E", C_HEAD);
    put_str(3, ORIGIN_X, "wasd - move    [ ] - speed    q - quit", C_TEXT);
    put_str(5, ORIGIN_X, "press any key to start", C_DIM);

    while (keyboard_poll() != 0) { }   /* drop whatever is still queued */

    /* seed from how long the player took to hit a key */
    volatile unsigned int seed = 1;
    while (keyboard_poll() == 0) seed++;
    rng = (unsigned int) seed | 1u;   /* xorshift must not start at zero */

    slen = 4;
    for (int i = 0; i < slen; i++) {
        sx[i] = (unsigned char) (FIELD_W / 2 - i);
        sy[i] = (unsigned char) (FIELD_H / 2);
    }
    dir = 1;
    score = 0;
    speed = SPEED_START;

    clear_screen();
    draw_border();
    draw_score();
    put_str(ORIGIN_Y + FIELD_H + 2, ORIGIN_X, "wasd move   [ ] speed   q quit", C_DIM);
    for (int i = 0; i < slen; i++) {
        draw_cell(sx[i], sy[i], i == 0 ? '@' : 'o', i == 0 ? C_HEAD : C_BODY);
    }
    place_food();

    int quit = 0;
    int dead = 0;

    while (!quit && !dead) {
        /* drain the queue and act on the newest key: mashing during a slow
         * tick shouldn't queue up turns that fire on later ticks */
        int k = 0;
        for (int q = keyboard_poll(); q != 0; q = keyboard_poll()) k = q;

        /* accept WASD as well as wasd: caps lock shouldn't break steering */
        if (k >= 'A' && k <= 'Z') k = k - 'A' + 'a';

        if (k == 'q') {
            quit = 1;
            break;
        } else if (k == 'w' && dir != 2) {
            dir = 0;
        } else if (k == 'd' && dir != 3) {
            dir = 1;
        } else if (k == 's' && dir != 0) {
            dir = 2;
        } else if (k == 'a' && dir != 1) {
            dir = 3;
        } else if (k == '[') {
            if (speed <= SPEED_MAX - SPEED_STEP) speed += SPEED_STEP;
        } else if (k == ']') {
            if (speed >= SPEED_MIN + SPEED_STEP) speed -= SPEED_STEP;
        }

        int nx = sx[0];
        int ny = sy[0];
        if (dir == 0) ny--;
        else if (dir == 1) nx++;
        else if (dir == 2) ny++;
        else nx--;

        if (nx < 0 || nx >= FIELD_W || ny < 0 || ny >= FIELD_H) {
            dead = 1;
            break;
        }
        for (int i = 0; i < slen; i++) {
            if (sx[i] == (unsigned char) nx && sy[i] == (unsigned char) ny) {
                dead = 1;
                break;
            }
        }
        if (dead) break;

        int grew = (nx == (int) food_x && ny == (int) food_y);

        if (grew) {
            score++;
            if (slen < MAX_LEN) {
                slen++;             /* the old tail cell stays drawn */
            } else {
                /* already at maximum length: move like a normal step */
                draw_cell(sx[slen - 1], sy[slen - 1], ' ', C_TEXT);
            }
        } else {
            draw_cell(sx[slen - 1], sy[slen - 1], ' ', C_TEXT);
        }

        for (int i = slen - 1; i > 0; i--) {
            sx[i] = sx[i - 1];
            sy[i] = sy[i - 1];
        }
        sx[0] = (unsigned char) nx;
        sy[0] = (unsigned char) ny;

        draw_cell(sx[0], sy[0], '@', C_HEAD);
        if (slen > 1) draw_cell(sx[1], sy[1], 'o', C_BODY);

        /* only now, with the body in its final state for this tick: placing
         * it earlier could drop food on the cell the head is moving into,
         * where the head would immediately paint over it */
        if (grew) place_food();

        draw_score();
        tick_delay();
    }

    if (dead) {
        char buf[12];
        vs_itoa(score, buf);
        put_str(ORIGIN_Y + FIELD_H / 2, ORIGIN_X + 24, " GAME OVER ", C_FOOD);
        put_str(ORIGIN_Y + FIELD_H / 2 + 1, ORIGIN_X + 24, " score: ", C_TEXT);
        put_str(ORIGIN_Y + FIELD_H / 2 + 1, ORIGIN_X + 32, buf, C_HEAD);
        put_str(ORIGIN_Y + FIELD_H + 2, ORIGIN_X, "press any key to leave        ", C_DIM);

        /* drop the keys they were holding as they died, or the screen
         * flashes past instantly */
        while (keyboard_poll() != 0) { }

        keyboard_getch();     /* now wait for a deliberate press */
    }

    while (keyboard_poll() != 0) { }   /* don't leak keys into the prompt */
    clear_screen();
}
