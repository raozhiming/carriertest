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

#include "errno.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "timedata.h"

void addnode(DataArray** list, int cnt, int value)
{
    DataArray* node = (DataArray*)malloc(sizeof(DataArray));
    if (NULL == node) return;

    node->consumeTime = value;
    node->next = NULL;

    if (0 == cnt) {
        *list = node;
        return;
    }

    DataArray *pTemp = *list;
    int i;
    for (i = 0; i < cnt - 1; i++) {
        pTemp = pTemp->next;
    }
    pTemp->next = node;
}

void insertnodebysort(DataArray** list, int cnt, int value)
{
    DataArray* node = (DataArray*)malloc(sizeof(DataArray));
    if (NULL == node) return;

    node->consumeTime = value;
    node->next = NULL;

    if (0 == cnt) {
        *list = node;
        return;
    }

    if (value <= (*list)->consumeTime) {
        node->next = *list;
        *list = node;
        return;
    }

    DataArray *pPre = *list;
    DataArray *pCur = pPre;
    int i;
    for (i = 0; i < cnt - 1; i++) {
        pCur = pPre->next;
        if (value <= pCur->consumeTime) {
            node->next = pCur;
            pPre->next = node;
            return;
        }
        pPre = pCur;
    }
    pPre->next = node;
}

void addData(TimeData * timedata, int value)
{
    addnode(&timedata->value_origin, timedata->count, value);
    insertnodebysort(&timedata->value_sort, timedata->count, value);
    timedata->count++;
}

void Dispose(TimeData* timedata)
{
    int i;
    DataArray *pTemp = (timedata)->value_origin;
    DataArray *pTemp2 = (timedata)->value_sort;
    for (i = 0; i < (timedata)->count; i++) {
        DataArray *pNext = pTemp->next;
        DataArray *pNext2 = pTemp2->next;

        free(pTemp);
        free(pTemp2);
        pTemp = pNext;
        pTemp2 = pNext2;
    }

    (timedata)->value_origin = NULL;
    (timedata)->value_sort = NULL;
    (timedata)->count = 0;
}

void getDate(char* dateStr)
{
    time_t timep;
    struct tm *p;
    time(&timep);
    p = localtime(&timep);

    sprintf(dateStr, "%04d%02d%02d%02d%02d", 1900 + p->tm_year, 1+p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min);
}

void OutputData(TimeData* timedata, const char* filename)
{
    char curTime[256] = {0}, backFileName[256] = {0};
    getDate(curTime);

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        return;
    }

    sprintf(backFileName, "%s-%s", curTime, filename);
    FILE *fpBackup = fopen(backFileName, "w");
    if (!fpBackup) {
        printf("Fail to fopen %s, %s\n", backFileName, strerror(errno));
    }

    int i, averageValue = 0;
    long long totalValue = 0;
    DataArray *pTemp = (timedata)->value_origin;
    DataArray *pTemp2 = (timedata)->value_sort;
    for (i = 0; i < (timedata)->count; i++) {
        fprintf(fp, "%d, %d, %d\n", i, pTemp->consumeTime, pTemp2->consumeTime);
        if (fpBackup) {
            fprintf(fpBackup, "%d, %d, %d\n", i, pTemp->consumeTime, pTemp2->consumeTime);
        }
        totalValue += pTemp2->consumeTime;
        pTemp = pTemp->next;
        pTemp2 = pTemp2->next;
    }

    averageValue = totalValue / timedata->count;
    fprintf(fp, "%d, \n", averageValue);
    if (fpBackup) {
        fprintf(fpBackup, "%d, \n", averageValue);
    }

    fclose(fp);
    if (fpBackup) fclose(fpBackup);
}