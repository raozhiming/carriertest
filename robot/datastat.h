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

#ifndef __SHELL_DATA_H__
#define __SHELL_DATA_H__

#include <stdbool.h>

typedef struct _DataArray
{
    int value;
    //定义指向链表中下一个结点的指针
    struct _DataArray* next;
 } DataArray;

typedef struct {
    int count;
    DataArray* value_origin;
    DataArray* value_sort;
} DataStat;

void addData(DataStat * timedata, int value1);
void OutputData(DataStat* timedata, const char* filename);
float CalConsumeTime(struct timeval start, struct timeval end);

void writeData(__time_t seconds, bool onLine, bool bStateChanged, char * filename, bool bClear);
#endif /* __SHELL_DATA_H__ */
