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
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <stdarg.h>
#include <getopt.h>
#include <limits.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <curses.h>
#include <sys/select.h>
#include <semaphore.h>

#include <confuse.h>
#include "config.h"

static char *spIndex = NULL;
static char serverAddress[ELA_MAX_ADDRESS_LEN+1];
static char serverUserID[ELA_MAX_ID_LEN+1];
static bool sNeedAddServer = false;
static bool sNeedRemoveServer = false;

static bool friends_list_callback(ElaCarrier *w, const ElaFriendInfo *friend_info, void *context);

void start_node(const char* app, int node_count, bool removeserver)
{
    char confIndex[32] = {0};
    char LD_LIBRARY_PATH[256] = {0};

    const char* psz_ld_path = getenv("LD_LIBRARY_PATH");
    sprintf(LD_LIBRARY_PATH, "LD_LIBRARY_PATH=%s", psz_ld_path);

    char *pArgRemove = NULL;
    if (removeserver) pArgRemove = "-r";

    char * const argvt[] = {app,confIndex, pArgRemove, NULL};
    char * const envp[ ]={LD_LIBRARY_PATH, NULL};

    int i;
    for(i = 0; i < node_count; i++) {
        printf("i = %d\n", i);
        sprintf(confIndex, "%d", i + 1);
        pid_t fpid=fork();
        if (fpid==0) {
            execve (app, argvt, envp);
        }
    }
}

void log_print(const char *format, va_list args)
{
    char string[256];
    vsprintf(string,format,args);
    printf("[%s]%s", "log_print", string);
}

//------------callback
static void idle_callback(ElaCarrier *w, void *context)
{
    // printf("idle_callback\n");
}

static void ready_callback(ElaCarrier *w, void *context)
{
    // printf("ready_callback\n");
}

static void connection_callback(ElaCarrier *w, ElaConnectionStatus status, void *context)
{
    switch (status) {
    case ElaConnectionStatus_Connected:
    //     gettimeofday(&friendOnline.middle_stamp, NULL);
    //     int timeuse = 1000 * (friendOnline.middle_stamp.tv_sec -friendOnline.start_stamp.tv_sec) + (friendOnline.middle_stamp.tv_usec - friendOnline.start_stamp.tv_usec) / 1000;
    //     addData(&testDataOnLine, timeuse);
        printf("[%4s] Connected to carrier network: \n", spIndex);
    //     writeData(friendOnline.middle_stamp.tv_sec, true, carrier_ctx.status != status, onLineMonitorFileName, false);
    //     cond_signal(&carrier_cond);
        if (sNeedAddServer) {
            int rc = ela_add_friend(w, serverAddress, "auto-reply");
            if (rc) {
                printf("ela_add_friend error (0x%x)\n", ela_get_error());
            }
        }

        if (sNeedRemoveServer) {
            int rc = ela_remove_friend(w, serverUserID);
            if (rc) {
                printf("ela_remove_friend error (0x%x)\n", ela_get_error());
            }
        }

        break;

    case ElaConnectionStatus_Disconnected:
        printf("[%4s] Disconnect from carrier network.\n", spIndex);
    //     gettimeofday(&friendOnline.start_stamp, NULL);
    //     writeData(friendOnline.start_stamp.tv_sec, false, carrier_ctx.status != status, onLineMonitorFileName, false);
    //     cond_reset(&carrier_cond);
        break;

    default:
        printf("Error!!! Got unknown connection status %d.\n", status);
    }

    // carrier_ctx.status = status;
}

static void friend_added_callback(ElaCarrier *w, const ElaFriendInfo *info, void *context)
{
    printf("friend_added_callback. The friend information:\n");
    sNeedAddServer = false;
    // display_friend_info(info);
}

static void friend_removed_callback(ElaCarrier *w, const char *friendid, void *context)
{
    printf("Friend %s removed!\n", friendid);
}

static int first_friends_item = 1;

static const char *connection_name[] = {
    "online",
    "offline"
};

static bool friends_list_callback(ElaCarrier *w, const ElaFriendInfo *friend_info, void *context)
{
    static int count;

    if (first_friends_item) {
        count = 0;
        printf("Friends list from carrier network:\n");
        printf("  %-46s %8s %s\n", "ID", "Connection", "Label");
        printf("  %-46s %8s %s\n", "----------------", "----------", "-----");
    }

    if (friend_info) {
        printf("  %-46s %8s %s\n", friend_info->user_info.userid,
               connection_name[friend_info->status], friend_info->label);
        first_friends_item = 0;
        count++;
    } else {
        /* The list ended */
        printf("Total %d friends.\n", count);

        first_friends_item = 1;
    }

    return true;
}

/* This callback share by list_friends and global
 * friend list callback */
static bool get_friends_callback(const ElaFriendInfo *friend_info, void *context)
{
    return true;
}

static void friend_info_callback(ElaCarrier *w, const char *friendid,
                                    const ElaFriendInfo *info, void *context)
{
    printf("Friend information changed:\n");
    // display_friend_info(info);
}

static void friend_connection_callback(ElaCarrier *w, const char *friendid,
                                    ElaConnectionStatus status, void *context)
{
    switch (status) {
    case ElaConnectionStatus_Connected:
        printf("+++ Friend[%s] connection changed to be online.\n", friendid);
        break;

    case ElaConnectionStatus_Disconnected:
        printf("--- Friend[%s] connection changed to be offline.\n", friendid);
        break;

    default:
        printf("Error!!! Got unknown connection status %d.\n", status);
    }
}

static void friend_presence_callback(ElaCarrier *w, const char *friendid,
                                     ElaPresenceStatus status,
                                     void *context)
{
    // if (status >= ElaPresenceStatus_None &&
    //     status <= ElaPresenceStatus_Busy) {
    //     output("Friend[%s] change presence to %s\n", friendid, presence_name[status]);
    // } else {
    //     output("Error!!! Got unknown presence status %d.\n", status);
    // }
}

static void friend_request_callback(ElaCarrier *w, const char *userid,
                                    const ElaUserInfo *info, const char *hello,
                                    void *context)
{
    printf("Friend request from user[%s] with HELLO: %s.\n",
           *info->name ? info->name : userid, hello);
    // output("Reply use following commands:\n");
    // output("  faccept %s\n", userid);
}

static void message_callback(ElaCarrier *w, const char *from,
                                    const char *msg, size_t len, void *context)
{
    ela_send_friend_message(w, from, msg, len);

    char timestr[20];
    time_t cur = time(NULL);
    strftime(timestr, 20, "%Y-%m-%d %H:%M:%S", localtime(&cur));
    printf("[%s] Message from [%s]: %d : %s\n", timestr, from, (int)len, msg);
    //parsemsg
}

static void invite_request_callback(ElaCarrier *w, const char *from,
                                    const char *data, size_t len, void *context)
{
    // output("Invite request from[%s] with data: %.*s\n", from, (int)len, data);
    // output("Reply use following commands:\n");
    // output("  ireply %s confirm [message]\n", from);
    // output("  ireply %s refuse [reason]\n", from);
}
//-----------

bool init_ElaOptions(ElaOptions* opts, ShellConfig *cfg)
{
    opts->udp_enabled = cfg->udp_enabled;
    opts->persistent_location = cfg->datadir;
    opts->bootstraps_size = cfg->bootstraps_size;
    opts->bootstraps = (BootstrapNode *)calloc(1, sizeof(BootstrapNode) * opts->bootstraps_size);
    if (!opts->bootstraps) {
        fprintf(stderr, "out of memory.");
        return false;
    }

    int i;
    for (i = 0 ; i < cfg->bootstraps_size; i++) {
        BootstrapNode *b = &opts->bootstraps[i];
        BootstrapNode *node = cfg->bootstraps[i];

        b->ipv4 = node->ipv4;
        b->ipv6 = node->ipv6;
        b->port = node->port;
        b->public_key = node->public_key;
    }
    return true;
}

bool init_ElaCallbacks(ElaCallbacks* callbacks)
{
    memset(callbacks, 0, sizeof(ElaCallbacks));
    callbacks->idle = idle_callback;
    callbacks->ready = ready_callback;
    callbacks->connection_status = connection_callback;
    callbacks->friend_list = friends_list_callback;
    callbacks->friend_connection = friend_connection_callback;
    callbacks->friend_info = friend_info_callback;
    callbacks->friend_presence = friend_presence_callback;
    callbacks->friend_request = friend_request_callback;
    callbacks->friend_added = friend_added_callback;
    callbacks->friend_removed = friend_removed_callback;
    callbacks->friend_message = message_callback;
    callbacks->friend_invite = invite_request_callback;
}

int main(int argc, char *argv[])
{
    char buffer[2048] = {0};
    char buf[ELA_MAX_ADDRESS_LEN+1];
    int wait_for_attach = 0, rc;
    bool bQuit = true;

    ShellConfig *cfg;
    ElaCarrier *w;
    ElaOptions opts;
    ElaCallbacks callbacks;

    printf("argc = %d\n", argc);
    if (0 == strcmp(argv[argc-1], "-r")) {
        sNeedRemoveServer = true;
        argc--;
    }
    else {
        sNeedRemoveServer = false;
    }

    TestConfig *testcfg;
    testcfg = load_test_config("carriertest.conf");
    if (!testcfg) {
        printf("loading configure failed !\n");
        return -1;
    }

    char *id = ela_get_id_by_address(testcfg->server_address, serverUserID, ELA_MAX_ID_LEN + 1);
    if (!id) {
        printf("server_address is invalid, pls check configure!\n");
        return -1;
    }
    strcpy(serverAddress, testcfg->server_address);

    if (argc < 2) {
        printf("config: node_count : %d, datadir:%s, server_address:%s\n", \
                testcfg->node_count, testcfg->datadir, testcfg->server_address);
        start_node(argv[0], testcfg->node_count - 1, sNeedRemoveServer);
        spIndex = "0";
    }
    else {
        spIndex = argv[1];
    }

    // printf("do something ----------%s\n", spIndex);

    cfg = load_config("carrier.conf", spIndex);
    if (!cfg) {
        fprintf(stderr, "loading carrier.conf failed !\n");
        return -1;
    }

    ela_log_init(cfg->loglevel, cfg->logfile, log_print);

    init_ElaOptions(&opts, cfg);
    init_ElaCallbacks(&callbacks);

    do {
        w = ela_new(&opts, &callbacks, NULL);
        // gettimeofday(&friendOnline.start_stamp, NULL);

        if (!w) {
            printf("Error create carrier instance: 0x%x\n", ela_get_error());
            goto quit;
        }

        printf("[%4s] Carrier node identities:\n", spIndex);
        // printf("   Node ID: %s\n", ela_get_nodeid(w, buf, sizeof(buf)));
        printf("   User ID: %s\n", ela_get_userid(w, buf, sizeof(buf)));
        printf("   Address: %s\n\n", ela_get_address(w, buf, sizeof(buf)));

        bool isfriend = ela_is_friend(w, serverUserID);
        if (!isfriend) {
            if (sNeedRemoveServer) {
                sNeedRemoveServer = false;
                sNeedAddServer = false;
            }
            else {
                sNeedAddServer = true;
            }
        }

        // session_init(w, 1, NULL);

        bQuit = true;
        // carrier_ctx.status = ElaConnectionStatus_Disconnected;
        rc = ela_run(w, 100);
        if (rc != 0) {
            printf("Error start carrier loop: 0x%x\n", ela_get_error());
            printf("Press any key to quit...");
            ela_kill(w);
            goto quit;
        }
        // output("loopcount: %d\n", loopcount);
        // if (loopcount++ > test_loopcnt) {
        //     loopcount = 0;
        //     bQuit = true;
        // }
    } while (!bQuit);

quit:
    ela_session_cleanup(w);
    deref(cfg);
    free(opts.bootstraps);
    printf("\n\n-----------------[%4s] quit-----------------\n\n", spIndex);
    return 0;
}
