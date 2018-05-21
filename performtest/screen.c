/*
 * Copyright (c) 2018 Elastos Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <curses.h>

#ifdef __linux__
#define __USE_GNU
#include <pthread.h>
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#else
#include <pthread.h>
#endif
#include "screen.h"

#define TIME_FORMAT     "%Y-%m-%d %H:%M:%S"

#define NUMBER_OF_HISTORY       256

#define OUTPUT_LINES   4

#define OUTPUT_WIN  1
#define LOG_WIN     2
#define CMD_WIN     3

static char default_data_location[PATH_MAX];
static const char *history_filename = ".elashell.history";

static char *cmd_history[NUMBER_OF_HISTORY];
static int cmd_history_last = 0;
static int cmd_history_cursor = 0;
static int cmd_cursor_dir = 1;
static bool cmd_init_sceen = false;

WINDOW *output_win_border, *output_win;
WINDOW *log_win_border, *log_win;
WINDOW *cmd_win_border, *cmd_win;

pthread_mutex_t screen_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;

void get_layout(int win, int *w, int *h, int *x, int *y)
{
    if (win == OUTPUT_WIN) {
        if (COLS < 100) {
            *x = 0;
            *y = LINES - (LINES -OUTPUT_LINES) / 2 - OUTPUT_LINES;

            *w = COLS;
            *h = (LINES -OUTPUT_LINES) / 2;
        } else {
            *x = 0;
            *y = 0;

            *w = (COLS - 1) / 2;
            *h = LINES - OUTPUT_LINES;
        }

        OUTPUT_COLS = *w -2;
    } else if (win == LOG_WIN) {
        if (COLS < 100) {
            *x = 0;
            *y = 0;

            *w = COLS;
            *h = LINES - (LINES -OUTPUT_LINES) / 2 - OUTPUT_LINES;
        } else {
            *x = COLS - (COLS / 2);
            *y = 0;

            *w = (COLS - 1) / 2;
            *h = LINES - OUTPUT_LINES;
        }
    } else if (win == CMD_WIN) {
        if (COLS < 100) {
            *x = 0;
            *y = LINES - OUTPUT_LINES;

            *w = COLS;
            *h = OUTPUT_LINES;
        } else {
            *x = 0;
            *y = LINES - OUTPUT_LINES;

            *w = COLS;
            *h = OUTPUT_LINES;
        }
    }
}

void handle_winch(int sig)
{
    int w, h, x, y;

    endwin();

    if (LINES < 20 || COLS < 80) {
        printf("Terminal size too small!\n");
        exit(-1);
    }

    refresh();
    clear();

    wresize(stdscr, LINES, COLS);

    get_layout(OUTPUT_WIN, &w, &h, &x, &y);

    wresize(output_win_border, h, w);
    mvwin(output_win_border, y, x);
    box(output_win_border, 0, 0);
    mvwprintw(output_win_border, 0, 4, "Output");

    wresize(output_win, h-2, w-2);
    mvwin(output_win, y+1, x+1);

    get_layout(LOG_WIN, &w, &h, &x, &y);

    wresize(log_win_border, h, w);
    mvwin(log_win_border, y, x);
    box(log_win_border, 0, 0);
    mvwprintw(log_win_border, 0, 4, "Log");

    wresize(log_win, h-2, w-2);
    mvwin(log_win, y+1,  x+1);

    get_layout(CMD_WIN, &w, &h, &x, &y);

    wresize(cmd_win_border, h, w);
    mvwin(cmd_win_border, y, x);
    box(cmd_win_border, 0, 0);
    mvwprintw(cmd_win_border, 0, 4, "Command");

    wresize(cmd_win, h-2, w-2);
    mvwin(cmd_win,  y+1,  x+1);

    clear();
    refresh();

    wrefresh(output_win_border);
    wrefresh(output_win);

    wrefresh(log_win_border);
    wrefresh(log_win);

    wrefresh(cmd_win_border);
    wrefresh(cmd_win);
}

void init_screen(void)
{
    int w, h, x, y;

    initscr();

    if (LINES < 20 || COLS < 80) {
        printf("Terminal size too small!\n");
        endwin();
        exit(-1);
    }

    noecho();
    nodelay(stdscr, TRUE);
    refresh();

    get_layout(OUTPUT_WIN, &w, &h, &x, &y);

    output_win_border = newwin(h, w, y, x);
    box(output_win_border, 0, 0);
    mvwprintw(output_win_border, 0, 4, "Output");
    wrefresh(output_win_border);

    output_win = newwin(h-2, w-2, y+1, x+1);
    scrollok(output_win, TRUE);
    wrefresh(output_win);

    get_layout(LOG_WIN, &w, &h, &x, &y);

    log_win_border = newwin(h, w, y, x);
    box(log_win_border, 0, 0);
    mvwprintw(log_win_border, 0, 4, "Log");
    wrefresh(log_win_border);

    log_win = newwin(h-2, w-2, y+1,  x+1);
    scrollok(log_win, TRUE);
    wrefresh(log_win);

    get_layout(CMD_WIN, &w, &h, &x, &y);

    cmd_win_border = newwin(h, w, y, x);
    box(cmd_win_border, 0, 0);
    mvwprintw(cmd_win_border, 0, 4, "Command");
    wrefresh(cmd_win_border);

    cmd_win = newwin(h-2, w-2, y+1,  x+1);
    scrollok(cmd_win, true);
    waddstr(cmd_win, "# ");
    wrefresh(cmd_win);

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = handle_winch;
    sigaction(SIGWINCH, &sa, NULL);

    cmd_init_sceen = true;
}

void cleanup_screen(void)
{
    endwin();

    delwin(output_win_border);
    delwin(output_win);

    delwin(log_win_border);
    delwin(log_win);

    delwin(cmd_win_border);
    delwin(cmd_win);
}

void history_load(void)
{
    int i = 0;
    char filename[PATH_MAX];
    FILE *fp;
    char line[1024];
    char *p;

    sprintf(filename, "%s/%s", default_data_location, history_filename);

    fp = fopen(filename, "r");
    if (!fp)
        return;

    while (fgets(line, sizeof(line), fp)) {
        // Trim trailing spaces;
        for (p = line + strlen(line) - 1; p >= line && isspace(*p); p--);
        *(++p) = 0;

        // Trim leading spaces;
        for (p = line; *p && isspace(*p); p++);

        if (strlen(p) == 0)
            continue;

        cmd_history[i] = strdup(p);

        i = (i + 1) % NUMBER_OF_HISTORY;
    }

    cmd_history_last = i;
    cmd_history_cursor = cmd_history_last;

    fclose(fp);
}

void history_save(void)
{
    int i = 0;
    char filename[PATH_MAX];
    FILE *fp;

    sprintf(filename, "%s/%s", default_data_location, history_filename);

    fp = fopen(filename, "w");
    if (!fp)
        return;

    i = cmd_history_last;
    do {
        if (cmd_history[i]) {
            fprintf(fp, "%s\n", cmd_history[i]);
            free(cmd_history[i]);
            cmd_history[i] = NULL;
        }

        i = (i + 1) % NUMBER_OF_HISTORY;
    } while (i != cmd_history_last);

    fclose(fp);
}

void history_add_cmd(const char *cmd)
{
    if (cmd_history[cmd_history_last])
        free(cmd_history[cmd_history_last]);

    cmd_history[cmd_history_last] = strdup(cmd);

    cmd_history_last = (cmd_history_last + 1) % NUMBER_OF_HISTORY;
    cmd_history_cursor = cmd_history_last;
    cmd_cursor_dir = 1;
}

const char *history_prev(void)
{
    int n;
    const char *cmd = NULL;

    if (cmd_cursor_dir == -1 &&
        (cmd_history_cursor == cmd_history_last ||
         cmd_history[cmd_history_cursor] == NULL))
        return NULL;

    n = (cmd_history_cursor - 1 + NUMBER_OF_HISTORY) % NUMBER_OF_HISTORY;
    cmd_history_cursor = n;

    if (cmd_history[n])
        cmd = cmd_history[n];

    cmd_cursor_dir = -1;

    return cmd;
}

const char *history_next(void)
{
    int n;
    const char *cmd = NULL;

    if (cmd_cursor_dir == 1 && cmd_history_cursor == cmd_history_last)
        return NULL;

    n = (cmd_history_cursor + 1) % NUMBER_OF_HISTORY;
    cmd_history_cursor = n;

    if (cmd_history_cursor != cmd_history_last)
        cmd = cmd_history[n];

    cmd_cursor_dir = 1;

    return cmd;
}

void log_print(const char *format, va_list args)
{
    if (!cmd_init_sceen)  {
        char string[256];
        vsprintf(string,format,args);
        printf("[%s]%s", "log_print", string);
    }
    else {
        pthread_mutex_lock(&screen_lock);
        vwprintw(log_win, format, args);
        wrefresh(log_win);
        wrefresh(cmd_win);
        pthread_mutex_unlock(&screen_lock);
    }
}

void output(const char *format, ...)
{
    va_list args;

    va_start(args, format);

    if (!cmd_init_sceen) {
        char string[256];
        vsprintf(string,format,args);
        printf("[%s]%s", "output", string);
    }
    else {
        pthread_mutex_lock(&screen_lock);
        vwprintw(output_win, format, args);
        wrefresh(output_win);
        wrefresh(cmd_win);
        pthread_mutex_unlock(&screen_lock);
    }

    va_end(args);
}

void outputEx(const char *format, ...)
{
    char timestr[20];
    char buf[1024];
    time_t cur = time(NULL);

    strftime(timestr, 20, TIME_FORMAT, localtime(&cur));

    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    buf[1023] = 0;

    output("%s : %s\n", timestr, buf);
}

void clearScreen(int type, char *screen)
{
    if (!cmd_init_sceen) return;

    if (type == 1) {
        pthread_mutex_lock(&screen_lock);
        wclear(output_win);
        wrefresh(output_win);
        wclear(log_win);
        wrefresh(log_win);
        wrefresh(cmd_win);
        pthread_mutex_unlock(&screen_lock);
    } else if (type == 2) {
        WINDOW *w;
        if (strcmp(screen, "log") == 0)
            w = log_win;
        else if (strcmp(screen, "out") == 0)
            w = output_win;
        else {
            output("Invalid command syntax.\n");
            return;
        }

        pthread_mutex_lock(&screen_lock);
        wclear(w);
        wrefresh(w);
        wrefresh(cmd_win);
        pthread_mutex_unlock(&screen_lock);
    }
}

char *read_cmd(void)
{
    int x, y;
    int w, h;
    int ch = 0;
    int rc;
    char *p;

    static int cmd_len = 0;
    static char cmd_line[1024];

    ch = getch();
    if (ch == -1)
        return NULL;

    getmaxyx(cmd_win, h, w);
    getyx(cmd_win, y, x);

    (void)h;

    pthread_mutex_lock(&screen_lock);
    if (ch == 10 || ch == 13) {
        rc = mvwinnstr(cmd_win, 0, 2, cmd_line, sizeof(cmd_line));
        mvwinnstr(cmd_win, 1, 0, cmd_line + rc, sizeof(cmd_line) - rc);

        wclear(cmd_win);
        waddstr(cmd_win, "# ");
        wrefresh(cmd_win);
        cmd_len = 0;

        // Trim trailing spaces;
        for (p = cmd_line + strlen(cmd_line) - 1; p >= cmd_line && isspace(*p); p--);
        *(++p) = 0;

        // Trim leading spaces;
        for (p = cmd_line; *p && isspace(*p); p++);

        if (strlen(p)) {
            history_add_cmd(p);
            pthread_mutex_unlock(&screen_lock);
            return p;
        }

    } else if (ch == 127) {
        if (cmd_len > 0 && y * w + x - 2 > 0) {
            if (x == 0) {
                x = w;
                y--;
            }
            wmove(cmd_win, y, x-1);
            wdelch(cmd_win);
            cmd_len--;
        }
    } else if (ch == 27) {
        getch();
        ch = getch();
        if (ch == 65 || ch == 66) {
            p = ch == 65 ? (char *)history_prev() : (char *)history_next();
            wclear(cmd_win);
            waddstr(cmd_win, "# ");
            if (p) waddstr(cmd_win, p);
            cmd_len = p ? (int)strlen(p) : 0;
        } /* else if (ch == 67) {
            if (y * w + x - 2 < cmd_len) {
                if (x == w-1) {
                    x = -1;
                    y++;
                }
                wmove(cmd_win, y, x+1);
            }
        } else if (ch == 68) {
            if (y * w + x - 2 > 0) {
                if (x == 0) {
                    x = w;
                    y--;
                }
                wmove(cmd_win, y, x-1);
            }
        }
        */
    } else {
        if (y * w + x - 2 >= cmd_len) {
            waddch(cmd_win, ch);
        } else {
            winsch(cmd_win, ch);
            wmove(cmd_win, y, x+1);
        }

        cmd_len++;
    }

    wrefresh(cmd_win);
    pthread_mutex_unlock(&screen_lock);

    return NULL;
}
