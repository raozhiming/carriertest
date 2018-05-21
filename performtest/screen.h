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

#ifndef __SHELL_SCREEN_H__
#define __SHELL_SCREEN_H__

int OUTPUT_COLS;

void get_layout(int win, int *w, int *h, int *x, int *y);
void handle_winch(int sig);
void init_screen(void);
void cleanup_screen(void);

void history_load(void);
void history_save(void);
void history_add_cmd(const char *cmd);
const char *history_prev(void);
const char *history_next(void);

void log_print(const char *format, va_list args);
void output(const char *format, ...);
void outputEx(const char *format, ...);
void clearScreen(int type, char * screen);

char *read_cmd(void);

#endif /* __SHELL_SCREEN_H__ */
