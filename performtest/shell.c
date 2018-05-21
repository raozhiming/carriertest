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
#include <sys/stat.h>
#include <sys/time.h>
#include <curses.h>
#include <sys/select.h>
#include <semaphore.h>

#include <ela_carrier.h>
#include <ela_session.h>

#include "cond.h"
#include "config.h"
#include "datastat.h"
#include "screen.h"

static struct {
    int disconnect_count;
    int offline_time;//second
    ElaConnectionStatus status;
    struct timeval first_connected;
    struct timeval last_connected;
} carrier_ctx;

static struct {
    ElaSession *ws;
    int unchanged_streams;
    char remote_sdp[2048];
    size_t sdp_len;
    int bulk_mode;
    size_t packets;
    size_t bytes;
    struct timeval first_stamp;
    struct timeval last_stamp;
} session_ctx;

static struct {
    struct timeval start_stamp;
    struct timeval middle_stamp;
    struct timeval end_stamp;
} friendOnline, addFriendTime;

static Condition DEFINE_COND(friendOnLine_cond);
static Condition DEFINE_COND(carrier_cond);
static Condition DEFINE_COND(session_cond);
char testFriendId[ELA_MAX_ID_LEN] = {0};

int loopcount = 0;
bool bQuit = true;
bool bOnlineTest = false;
bool bSessionTest = false;
bool bAddFriendTest = false;
int test_loopcnt = 0;
int gSessionSendBytes = 0;
DataStat testDataOnLine = {0};
DataStat testDataFriendOnLine = {0};
DataStat testDataAddFriend = {0};
DataStat testDataSessionSpeed = {0};
ElaStreamState streamState;
sem_t session_status;

bool gServerAutoTest = true;

static int stream_add(ElaCarrier *w, int argc, char *argv[]);
static int stream_addByOption(ElaCarrier *w, int options);
static void session_reply_request(ElaCarrier *w, int argc, char *argv[]);

static void clear_screen(ElaCarrier *w, int argc, char *argv[])
{
    if (argc == 1) {
        clearScreen(1, NULL);
    } else if (argc == 2) {
        clearScreen(2, argv[1]);
    } else {
        output("Invalid command syntax.\n");
        return;
    }
}

static void get_address(ElaCarrier *w, int argc, char *argv[])
{
    if (argc != 1) {
        output("Invalid command syntax.\n");
        return;
    }

    char addr[ELA_MAX_ADDRESS_LEN+1] = {0};
    ela_get_address(w, addr, sizeof(addr));
    output("Address: %s\n", addr);
}

static void get_nodeid(ElaCarrier *w, int argc, char *argv[])
{
    if (argc != 1) {
        output("Invalid command syntax.\n");
        return;
    }

    char id[ELA_MAX_ID_LEN+1] = {0};
    ela_get_nodeid(w, id, sizeof(id));
    output("Node ID: %s\n", id);
}

static void get_userid(ElaCarrier *w, int argc, char *argv[])
{
    if (argc != 1) {
        output("Invalid command syntax.\n");
        return;
    }

    char id[ELA_MAX_ID_LEN+1] = {0};
    ela_get_userid(w, id, sizeof(id));
    output("User ID: %s\n", id);
}

static void display_user_info(const ElaUserInfo *info)
{
    output("           ID: %s\n", info->userid);
    output("         Name: %s\n", info->name);
    output("  Description: %s\n", info->description);
    output("       Gender: %s\n", info->gender);
    output("        Phone: %s\n", info->phone);
    output("        Email: %s\n", info->email);
    output("       Region: %s\n", info->region);
}

static void self_info(ElaCarrier *w, int argc, char *argv[])
{
    ElaUserInfo info;
    int rc;

    rc = ela_get_self_info(w, &info);
    if (rc != 0) {
        output("Get self information failed(0x%x).\n", ela_get_error());
        return;
    }

    if (argc == 1) {
        output("Self information:\n");
        display_user_info(&info);
    } else if (argc == 3 || argc == 4) {
        const char *value = "";
        if (strcmp(argv[1], "set") != 0) {
            output("Unknown sub command: %s\n", argv[1]);
            return;
        }

        if (argc == 4) {
            value = argv[3];
        }

        if (strcmp(argv[2], "name") == 0) {
            strncpy(info.name, value, sizeof(info.name));
            info.name[sizeof(info.name)-1] = 0;
        } else if (strcmp(argv[2], "description") == 0) {
            strncpy(info.description, value, sizeof(info.description));
            info.description[sizeof(info.description)-1] = 0;
        } else if (strcmp(argv[2], "gender") == 0) {
            strncpy(info.gender, value, sizeof(info.gender));
            info.gender[sizeof(info.gender)-1] = 0;
        } else if (strcmp(argv[2], "phone") == 0) {
            strncpy(info.phone, value, sizeof(info.phone));
            info.phone[sizeof(info.phone)-1] = 0;
        } else if (strcmp(argv[2], "email") == 0) {
            strncpy(info.email, value, sizeof(info.email));
            info.email[sizeof(info.email)-1] = 0;
        } else if (strcmp(argv[2], "region") == 0) {
            strncpy(info.region, value, sizeof(info.region));
            info.region[sizeof(info.region)-1] = 0;
        } else {
            output("Invalid attribute name: %s\n", argv[2]);
            return;
        }

        ela_set_self_info(w, &info);
    } else {
        output("Invalid command syntax.\n");
    }
}

static void self_nospam(ElaCarrier *w, int argc, char *argv[])
{
    uint32_t nospam;

    if (argc == 1) {
        ela_get_self_nospam(w, &nospam);
        output("Self nospam: %lu.\n", nospam);
        return;
    } else if (argc == 2) {
        nospam = (uint32_t)atoi(argv[1]);
        if (nospam == 0) {
            output("Invalid nospam value to set.");
            return;
        }

        ela_set_self_nospam(w, nospam);
        output("User's nospam changed to be %lu.\n", nospam);
    } else {
        output("Invalid command syntax.\n");
    }
}

const char *presence_name[] = {
    "none",    // None;
    "away",    // Away;
    "busy",    // Busy;
};

static void self_presence(ElaCarrier *w, int argc, char *argv[])
{
    ElaPresenceStatus presence;
    int rc;

    if (argc == 1) {
        ela_get_self_presence(w, &presence);
        output("Self presence: %s\n", presence_name[presence]);
        return;
    } else if (argc == 2) {
        if (strcmp(argv[1], "none") == 0)
            presence = ElaPresenceStatus_None;
        else if (strcmp(argv[1], "away") == 0)
            presence = ElaPresenceStatus_Away;
        else if (strcmp(argv[1], "busy") == 0)
            presence = ElaPresenceStatus_Busy;
        else {
            output("Invalid command syntax.\n");
            return;
        }

        rc = ela_set_self_presence(w, presence);
        if (rc < 0)
            output("Set user's presence failed (0x%x)\n", ela_get_error());
        else
            output("User's presence changed to be %s.\n", argv[1]);
    } else {
        output("Invalid command syntax.\n");
    }
}

static void friend_add(ElaCarrier *w, int argc, char *argv[])
{
    int rc;

    if (argc != 3) {
        output("Invalid command syntax.\n");
        return;
    }

    if (!ela_address_is_valid(argv[1])) {
        output("address is Invalid, pls check args.\n");
        return;
    }

    rc = ela_add_friend(w, argv[1], argv[2]);
    if (rc == 0)
        output("Request to add a new friend succeess.\n");
    else
        output("Request to add a new friend failed(0x%x).\n", ela_get_error());
}

static void friend_accept(ElaCarrier *w, int argc, char *argv[])
{
    int rc;

    if (argc != 2) {
        output("Invalid command syntax.\n");
        return;
    }

    rc = ela_accept_friend(w, argv[1]);
    if (rc == 0)
        output("Accept friend request success.\n");
    else
        output("Accept friend request failed(0x%x).\n", ela_get_error());
}

static void friend_remove(ElaCarrier *w, int argc, char *argv[])
{
    int rc;

    if (argc != 2) {
        output("Invalid command syntax.\n");
        return;
    }

    rc = ela_remove_friend(w, argv[1]);
    if (rc == 0)
        output("Remove friend %s success.\n", argv[1]);
    else
        output("Remove friend %s failed (0x%x).\n", argv[1], ela_get_error());
}

static int first_friends_item = 1;

static const char *connection_name[] = {
    "online",
    "offline"
};

/* This callback share by list_friends and global
 * friend list callback */
static bool friends_list_callback(ElaCarrier *w, const ElaFriendInfo *friend_info,
                                 void *context)
{
    static int count;

    output("ElaCallbacksfriend_list...\n");

    if (first_friends_item) {
        count = 0;
        output("Friends list from carrier network:\n");
        output("  %-46s %8s %s\n", "ID", "Connection", "Label");
        output("  %-46s %8s %s\n", "----------------", "----------", "-----");
    }

    if (friend_info) {
        output("  %-46s %8s %s\n", friend_info->user_info.userid,
               connection_name[friend_info->status], friend_info->label);
        first_friends_item = 0;
        count++;
    } else {
        /* The list ended */
        output("  ----------------\n");
        output("Total %d friends.\n", count);

        first_friends_item = 1;
    }

    return true;
}

/* This callback share by list_friends and global
 * friend list callback */
static bool get_friends_callback(const ElaFriendInfo *friend_info, void *context)
{
    static int count;

    output("ElaFriendsIterateCallback...\n");

    if (first_friends_item) {
        count = 0;
        output("Friends list:\n");
        output("  %-46s %8s %s\n", "ID", "Connection", "Label");
        output("  %-46s %8s %s\n", "----------------", "----------", "-----");
    }

    if (friend_info) {
        output("  %-46s %8s %s\n", friend_info->user_info.userid,
               connection_name[friend_info->status], friend_info->label);
        first_friends_item = 0;
        count++;
    } else {
        /* The list ended */
        output("  ----------------\n");
        output("Total %d friends.\n", count);

        first_friends_item = 1;
    }

    return true;
}

static void list_friends(ElaCarrier *w, int argc, char *argv[])
{
    if (argc != 1) {
        output("Invalid command syntax.\n");
        return;
    }

    ela_get_friends(w, get_friends_callback, NULL);
}

static void display_friend_info(const ElaFriendInfo *fi)
{
    output("           ID: %s\n", fi->user_info.userid);
    output("         Name: %s\n", fi->user_info.name);
    output("  Description: %s\n", fi->user_info.description);
    output("       Gender: %s\n", fi->user_info.gender);
    output("        Phone: %s\n", fi->user_info.phone);
    output("        Email: %s\n", fi->user_info.email);
    output("       Region: %s\n", fi->user_info.region);
    output("        Label: %s\n", fi->label);
    output("     Presence: %s\n", presence_name[fi->presence]);
    output("   Connection: %s\n", connection_name[fi->status]);
}

static void show_friend(ElaCarrier *w, int argc, char *argv[])
{
    int rc;
    ElaFriendInfo fi;

    if (argc != 2) {
        output("Invalid command syntax.\n");
        return;
    }

    rc = ela_get_friend_info(w, argv[1], &fi);
    if (rc < 0) {
        output("Get friend information failed(0x%x).\n", ela_get_error());
        return;
    }

    output("Friend %s information:\n", argv[1]);
    display_friend_info(&fi);
}

static void label_friend(ElaCarrier *w, int argc, char *argv[])
{
    int rc;

    if (argc != 3) {
        output("Invalid command syntax.\n");
        return;
    }

    rc = ela_set_friend_label(w, argv[1], argv[2]);
    if (rc == 0)
        output("Update friend label success.\n");
    else
        output("Update friend label failed(0x%x).\n", ela_get_error());
}

static void friend_added_callback(ElaCarrier *w, const ElaFriendInfo *info,
                                  void *context)
{
    output("New friend added. The friend information:\n");
    display_friend_info(info);
}

static void friend_removed_callback(ElaCarrier *w, const char *friendid,
                                    void *context)
{
    output("Friend %s removed!\n", friendid);
}

static int send_message(ElaCarrier *w, int argc, char *argv[])
{
    int rc;

    if (argc != 3) {
        output("Invalid command syntax.\n");
        return -1;
    }

    rc = ela_send_friend_message(w, argv[1], argv[2], strlen(argv[2]) + 1);
    if (rc == 0)
        output("Send message success.\n");
    else
        output("Send message failed(0x%x).\n", ela_get_error());
    return rc;
}

static void invite_response_callback(ElaCarrier *w, const char *friendid,
                                     int status, const char *reason,
                                     const char *data, size_t len, void *context)
{
    output("Got invite response from %s. ", friendid);
    if (status == 0) {
        output("message within response: %.*s\n", (int)len, data);
    } else {
        output("refused: %s\n", reason);
    }
}

static void invite(ElaCarrier *w, int argc, char *argv[])
{
    int rc;

    if (argc != 3) {
        output("Invalid command syntax.\n");
        return;
    }

    rc = ela_invite_friend(w, argv[1], argv[2], strlen(argv[2]),
                               invite_response_callback, NULL);
    if (rc == 0)
        output("Send invite request success.\n");
    else
        output("Send invite request failed(0x%x).\n", ela_get_error());
}

static void reply_invite(ElaCarrier *w, int argc, char *argv[])
{
    int rc;
    int status = 0;
    const char *reason = NULL;
    const char *msg = NULL;
    size_t msg_len = 0;

    if (argc != 4) {
        output("Invalid command syntax.\n");
        return;
    }

    if (strcmp(argv[2], "confirm") == 0) {
        msg = argv[3];
        msg_len = strlen(argv[3]);
    } else if (strcmp(argv[2], "refuse") == 0) {
        status = -1; // TODO: fix to correct status code.
        reason = argv[3];
    } else {
        output("Unknown sub command: %s\n", argv[2]);
        return;
    }

    rc = ela_reply_friend_invite(w, argv[1], status, reason, msg, msg_len);
    if (rc == 0)
        output("Send invite reply to inviter success.\n");
    else
        output("Send invite reply to inviter failed(0x%x).\n", ela_get_error());
}

static void kill_carrier(ElaCarrier *w, int argc, char *argv[])
{
    ela_kill(w);
}

static void session_request_callback(ElaCarrier *w, const char *from,
            const char *sdp, size_t len, void *context)
{
    strncpy(session_ctx.remote_sdp, sdp, len);
    session_ctx.remote_sdp[len] = 0;
    session_ctx.sdp_len = len;

    output("ElaSessionRequestCallback from[%s] with SDP:\n%s.\n", from, session_ctx.remote_sdp);
    output("Reply use following commands:\n");
    output("  sreply refuse [reason]\n");
    output("OR:\n");
    output("  1. snew %s\n", from);
    output("  2. sadd [plain] [reliable] [multiplexing] [portforwarding]\n");
    output("  3. sreply ok\n");
    if (gServerAutoTest) {
        char *args_addstream[2] = {"", "reliable"};
        char *args_reply[2] = {"", "ok"};

        session_ctx.ws = ela_session_new(w, from);
        if (!session_ctx.ws) {
            output("Create session failed. userid:%s\n", from);
        } else {
            output("Create session successfully.\n");
        }
        session_ctx.unchanged_streams = 0;
        int streamId = stream_add(w, 2, args_addstream);//close streamid
        output("streamId = %d.\n", streamId);

        output("session_request_callback sem_wait\n");
        sem_wait(&session_status);
        session_reply_request(w, 2, args_reply);
    }
}

static void session_request_complete_callback(ElaSession *ws, int status,
                const char *reason, const char *sdp, size_t len, void *context)
{
    int rc;

    output("ElaSessionRequestCompleteCallback, status: %d, reason: %s\n", status,
           reason ? reason : "null");

    if (status != 0)
        return;

    strncpy(session_ctx.remote_sdp, sdp, len);
    session_ctx.remote_sdp[len] = 0;
    session_ctx.sdp_len = len;

    rc = ela_session_start(session_ctx.ws, session_ctx.remote_sdp,
                               session_ctx.sdp_len);

    output("Session start %s.\n", rc == 0 ? "success" : "failed");
    if (gServerAutoTest) {
        session_ctx.bulk_mode = 1;
        session_ctx.bytes = 0;
        session_ctx.packets = 0;
    }
}

static void resetSessionCtx(int mode)
{
    session_ctx.bulk_mode = mode;
    session_ctx.bytes = 0;
    session_ctx.packets = 0;
}

static void showSessionSpeed()
{
    struct timeval start = session_ctx.first_stamp;
    struct timeval end = session_ctx.last_stamp;

    int duration = (int)((end.tv_sec - start.tv_sec) * 1000000 +
                         (end.tv_usec - start.tv_usec)) / 1000;
    duration = (duration == 0)  ? 1 : duration;
    float speed = ((session_ctx.bytes / duration) * 1000) / 1024;
    addData(&testDataSessionSpeed, (int)speed);

    output("\nFinish! Total %" PRIu64 " bytes(%d packets) in %d.%03d seconds. %.2f KB/s\n",
           session_ctx.bytes, session_ctx.packets,
           (int)(duration / 1000), (int)(duration % 1000), speed);
}

static void stream_bulk_receive(ElaCarrier *w, int argc, char *argv[])
{
    if (argc != 2) {
        output("Invalid command syntax.\n");
        return;
    }

    if (strcmp(argv[1], "start") == 0) {
        resetSessionCtx(0);

        output("Ready to receive bulk data");
    } else if (strcmp(argv[1], "end") == 0) {
        showSessionSpeed();

        resetSessionCtx(0);
    } else  {
        output("Invalid command syntax.\n");
        return;
    }
}

static void stream_on_data(ElaSession *ws, int stream, const void *data,
                           size_t len, void *context)
{
    if (session_ctx.bulk_mode) {
        if (session_ctx.packets % 1000 == 0)
            output(".");

        if (session_ctx.packets)
            gettimeofday(&session_ctx.last_stamp, NULL);
        else
            gettimeofday(&session_ctx.first_stamp, NULL);

        session_ctx.bytes += len;
        session_ctx.packets++;

        if (bSessionTest && (session_ctx.bytes >= gSessionSendBytes)) {
            //receive all data
            showSessionSpeed();
            resetSessionCtx(1);
            output("stream_on_data end, post\n");
            sem_post(&session_status);
        }
    } else {
        if (gServerAutoTest) {
            output("Stream [%d] received data len:%d\n", stream, (int)len);
        }
        else {
            output("Stream [%d] received data [%.*s]\n", stream, (int)len, (char*)data);
        }
    }
}

static void stream_on_state_changed(ElaSession *ws, int stream,
        ElaStreamState state, void *context)
{
    const char *state_name[] = {
        "raw",
        "initialized",
        "transport_ready",
        "connecting",
        "connected",
        "deactivated",
        "closed",
        "failed"
    };

    streamState = state;

    output("ElaStreamCallbacksstate_changed [%d] state changed to: %s\n", stream, state_name[state]);
    if ((state == ElaStreamState_initialized) || (state == ElaStreamState_connected) || (state == ElaStreamState_failed)) {
        output("stream_on_state_changed sem_post\n");
        sem_post(&session_status);
    }

    if (state == ElaStreamState_transport_ready)
        --session_ctx.unchanged_streams;
}

bool on_channel_open(ElaSession *ws, int stream, int channel,
                     const char *cookie, void *context)
{
    output("ElaStreamCallbackschannel_open %d.\n", channel);
    return true;
}

void on_channel_opened(ElaSession *ws, int stream, int channel, void *context)
{
    output("ElaStreamCallbackschannel_opened stream: %d, channel:%d opened.\n", stream, channel);
}

void on_channel_close(ElaSession *ws, int stream, int channel,
                      CloseReason reason, void *context)
{
    output("ElaStreamCallbackschannel_close stream: %d, channel:%d closed.\n", stream, channel);
}

bool on_channel_data(ElaSession *ws, int stream, int channel,
                     const void *data, size_t len, void *context)
{
    output("ElaStreamCallbackschannel_data stream: %d, channel:%d received data [%s]\n", stream, channel, data);
    return true;
}

void on_channel_pending(ElaSession *ws, int stream, int channel, void *context)
{
    output("ElaStreamCallbackschannel_pending stream: %d, channel:%d is pending.\n", stream, channel);
}

void on_channel_resume(ElaSession *ws, int stream, int channel, void *context)
{
    output("ElaStreamCallbackschannel_resume stream: %d, channel:%d resumed.\n", stream, channel);
}

static void session_init(ElaCarrier *w, int argc, char *argv[])
{
    int rc = ela_session_init(w, session_request_callback, NULL);
    if (rc < 0) {
        output("Session initialized failed.\n");
    }
    else {
        output("Session initialized successfully.\n");
    }
}

static void session_cleanup(ElaCarrier *w, int argc, char *argv[])
{
    if (argc != 1) {
        output("Invalid command syntax.\n");
        return;
    }

    ela_session_cleanup(w);
    output("Session cleaned up.\n");
}

static void session_new(ElaCarrier *w, int argc, char *argv[])
{
    if (argc != 2) {
        output("Invalid command syntax.\n");
        return;
    }

    session_ctx.ws = ela_session_new(w, argv[1]);
    if (!session_ctx.ws) {
        output("Create session failed.\n");
    } else {
        output("Create session successfully.\n");
    }
    session_ctx.unchanged_streams = 0;
}

static void session_close(ElaCarrier *w, int argc, char *argv[])
{
    if (argc != 1) {
        output("Invalid command syntax.\n");
        return;
    }

    if (session_ctx.ws) {
        ela_session_close(session_ctx.ws);
        session_ctx.ws = NULL;
        session_ctx.remote_sdp[0] = 0;
        session_ctx.sdp_len = 0;
        output("Session closed.\n");
    } else {
        output("No session available.\n");
    }
}

static int stream_add(ElaCarrier *w, int argc, char *argv[])
{
    int options = 0;

    if (argc < 1) {
        output("Invalid command syntax.\n");
        return -1;
    } else if (argc > 1) {
        int i;

        for (i = 1; i < argc; i++) {
            if (strcmp(argv[i], "reliable") == 0) {
                options |= ELA_STREAM_RELIABLE;
            } else if (strcmp(argv[i], "plain") == 0) {
                options |= ELA_STREAM_PLAIN;
            } else if (strcmp(argv[i], "multiplexing") == 0) {
                options |= ELA_STREAM_MULTIPLEXING;
            } else if (strcmp(argv[i], "portforwarding") == 0) {
                options |= ELA_STREAM_PORT_FORWARDING;
            } else {
                output("Invalid command syntax.\n");
                return -1;
            }
        }
    }

    return stream_addByOption(w, options);
}

static int stream_addByOption(ElaCarrier *w, int options)
{
    int rc;

    ElaStreamCallbacks callbacks;

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.state_changed = stream_on_state_changed;
    callbacks.stream_data = stream_on_data;

    // if ((options & ELA_STREAM_MULTIPLEXING) || (options & ELA_STREAM_PORT_FORWARDING)) {
        callbacks.channel_open = on_channel_open;
        callbacks.channel_opened = on_channel_opened;
        callbacks.channel_data = on_channel_data;
        callbacks.channel_pending = on_channel_pending;
        callbacks.channel_resume = on_channel_resume;
        callbacks.channel_close = on_channel_close;
    // }

    rc = ela_session_add_stream(session_ctx.ws, ElaStreamType_text,
                                options, &callbacks, NULL);
    if (rc < 0) {
        output("Add stream failed. ec:%x\n", ela_get_error());
    }
    else {
        session_ctx.unchanged_streams++;
        output("Add stream successfully and stream id %d.\n", rc);
    }

    return rc;
}

static void stream_remove(ElaCarrier *w, int argc, char *argv[])
{
    int rc;
    int stream;

    if (argc != 2) {
        output("Invalid command syntax.\n");
        return;
    }

    stream = atoi(argv[1]);
    rc = ela_session_remove_stream(session_ctx.ws, stream);
    if (rc < 0) {
        output("Remove stream %d failed.\n", stream);
    }
    else {
        output("Remove stream %d success.\n", stream);
    }
}

static void session_request(ElaCarrier *w, int argc, char *argv[])
{
    int rc;

    if (argc != 1) {
        output("Invalid command syntax.\n");
        return;
    }

    rc = ela_session_request(session_ctx.ws,
                             session_request_complete_callback, NULL);
    if (rc < 0) {
        output("session request failed. rc = %x\n", rc);
    }
    else {
        output("session request successfully.\n");
    }
}

static void session_reply_request(ElaCarrier *w, int argc, char *argv[])
{
    int rc;

    if ((argc != 2) && (argc != 3)) {
        output("Invalid command syntax.\n");
        return;
    }

    if ((strcmp(argv[1], "ok") == 0) && (argc == 2)) {
        rc = ela_session_reply_request(session_ctx.ws, 0, NULL);
        if (rc < 0) {
            output("response invite failed.\n");
        }
        else {
            output("response invite successuflly.\n");

            while (session_ctx.unchanged_streams > 0)
                usleep(200);

            rc = ela_session_start(session_ctx.ws, session_ctx.remote_sdp,
                                       session_ctx.sdp_len);

            output("Session start %s.\n", rc == 0 ? "success" : "failed");
        }
    }
    else if ((strcmp(argv[1], "refuse") == 0) && (argc == 3)) {
        rc = ela_session_reply_request(session_ctx.ws, 1, argv[2]);
        if (rc < 0) {
            output("response invite failed.\n");
        }
        else {
            output("response invite successuflly.\n");
        }
    }
    else {
        output("Unknown sub command.\n");
        return;
    }
}

static void stream_write(ElaCarrier *w, int argc, char *argv[])
{
    ssize_t rc;

    if (argc != 3) {
        output("Invalid command syntax.\n");
        return;
    }

    rc = ela_stream_write(session_ctx.ws, atoi(argv[1]),
                              argv[2], strlen(argv[2]) + 1);
    if (rc < 0) {
        output("write data failed.\n");
    }
    else {
        output("write data successfully.\n");
    }
}

struct bulk_write_args {
    int stream;
    int packet_size;
    int packet_count;
};
struct bulk_write_args args;

static void *bulk_write_thread(void *arg)
{
    ssize_t rc;
    int i;
    char *packet;
    struct bulk_write_args *args = (struct bulk_write_args *)arg;
    struct timeval start, end;
    int duration;
    float speed;

    packet = (char *)alloca(args->packet_size);
    memset(packet, 'D', args->packet_size);

    output("Begin sending data");

    gettimeofday(&start, NULL);
    for (i = 0; i < args->packet_count; i++) {
        size_t total = args->packet_size;
        size_t sent = 0;
        // output(" %d\n", i);

        do {
            // output("^");
            rc = ela_stream_write(session_ctx.ws, args->stream,
                                      packet + sent, total - sent);
            if (rc == 0) {
                usleep(100);
                continue;
            } else if (rc < 0) {
                if (ela_get_error() == ELA_GENERAL_ERROR(ELAERR_BUSY)) {
                    usleep(100);
                    continue;
                } else {
                    output("\nWrite data failed.\n");
                    return NULL;
                }
            }

            sent += rc;
        } while (sent < total);

        if (i % 1000 == 0)
            output(".");
    }
    gettimeofday(&end, NULL);

    duration = (int)((end.tv_sec - start.tv_sec) * 1000000 +
                     (end.tv_usec - start.tv_usec)) / 1000;
    duration = (duration == 0) ? 1 : duration;
    speed = (((args->packet_size * args->packet_count) / duration) * 1000) / 1024;

    output("\nFinish! Total %" PRIu64 " bytes in %d.%03d seconds. %.2f KB/s\n",
           (uint64_t)(args->packet_size * args->packet_count),
           (int)(duration / 1000), (int)(duration % 1000), speed);
    output("bulk_write_thread sem_post\n");
    sem_post(&session_status);

    return NULL;
}

static void stream_bulk_write(ElaCarrier *w, int argc, char *argv[])
{
    pthread_attr_t attr;
    pthread_t th;

    if (argc != 4) {
        output("Invalid command syntax.\n");
        return;
    }

    args.stream = atoi(argv[1]);
    args.packet_size = atoi(argv[2]);
    args.packet_count = atoi(argv[3]);
    if (args.stream <= 0 || args.packet_size <= 0 || args.packet_count <= 0) {
        output("Invalid command syntax.\n");
        return;
    }

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&th, &attr, bulk_write_thread, &args);
    pthread_attr_destroy(&attr);
}

static void stream_get_info(ElaCarrier *w, int argc, char *argv[])
{
    int rc;
    ElaTransportInfo info;

    const char *topology_name[] = {
        "LAN",
        "P2P",
        "RELAYED"
    };

    const char *addr_type[] = {
        "HOST   ",
        "SREFLEX",
        "PREFLEX",
        "RELAY  "
    };

    if (argc != 2) {
        output("Invalid command syntax.\n");
        return;
    }

    rc = ela_stream_get_transport_info(session_ctx.ws, atoi(argv[1]), &info);
    if (rc < 0) {
        output("get remote addr failed.\n");
        return;
    }

    output("Stream transport information:\n");
    output("    Network: %s\n", topology_name[info.topology]);

    output("      Local: %s %s:%d", addr_type[info.local.type], info.local.addr, info.local.port);
    if (*info.local.related_addr)
        output(" related %s:%d\n", info.local.related_addr, info.local.related_port);
    else
        output("\n");

    output("     Remote: %s %s:%d", addr_type[info.remote.type], info.remote.addr, info.remote.port);
    if (*info.remote.related_addr)
        output(" related %s:%d\n", info.remote.related_addr, info.remote.related_port);
    else
        output("\n");
}

static void stream_add_channel(ElaCarrier *w, int argc, char *argv[])
{
    int ch;

    if (argc != 2) {
        output("Invalid command syntax.\n");
        return;
    }

    ch = ela_stream_open_channel(session_ctx.ws, atoi(argv[1]), NULL);
    if (ch <= 0) {
        output("Create channel failed.\n");
    } else {
        output("Channel %d created.\n", ch);
    }
}

static void stream_close_channel(ElaCarrier *w, int argc, char *argv[])
{
    int rc;

    if (argc != 3) {
        output("Invalid command syntax.\n");
        return;
    }

    rc = ela_stream_close_channel(session_ctx.ws, atoi(argv[1]), atoi(argv[2]));
    if (rc < 0) {
        output("Close channel failed.\n");
    } else {
        output("Channel %s closed.\n", argv[2]);
    }
}

static void stream_write_channel(ElaCarrier *w, int argc, char *argv[])
{
    ssize_t rc;

    if (argc != 4) {
        output("Invalid command syntax.\n");
        return;
    }

    rc = ela_stream_write_channel(session_ctx.ws, atoi(argv[1]),
            atoi(argv[2]), argv[3], strlen(argv[3])+1);
    if (rc < 0) {
        output("Write channel failed.\n");
    } else {
        output("Channel %s write successfully.\n", argv[2]);
    }
}

static void stream_pend_channel(ElaCarrier *w, int argc, char *argv[])
{
    ssize_t rc;

    if (argc != 3) {
        output("Invalid command syntax.\n");
        return;
    }

    rc = ela_stream_pend_channel(session_ctx.ws, atoi(argv[1]),
                                     atoi(argv[2]));
    if (rc < 0) {
        output("Pend channel(input) failed.\n");
    } else {
        output("Channel %s input is pending.\n", argv[2]);
    }
}

static void stream_resume_channel(ElaCarrier *w, int argc, char *argv[])
{
    ssize_t rc;

    if (argc != 3) {
        output("Invalid command syntax.\n");
        return;
    }

    rc = ela_stream_resume_channel(session_ctx.ws, atoi(argv[1]),
                                       atoi(argv[2]));

    if (rc < 0) {
        output("Resume channel(input) failed.\n");
    } else {
        output("Channel %s input is resumed.\n", argv[2]);
    }
}

static void session_add_service(ElaCarrier *w, int argc, char *argv[])
{
    PortForwardingProtocol protocol;
    int rc;

    if (argc != 5) {
        output("Invalid command syntax.\n");
        return;
    }

    if (strcmp(argv[2], "tcp") == 0)
        protocol = PortForwardingProtocol_TCP;
    else {
        output("Unknown protocol %s.\n", argv[2]);
        return;
    }

    rc = ela_session_add_service(session_ctx.ws, argv[1],
                                     protocol, argv[3], argv[4]);
    output("Add service %s %s.\n", argv[1], rc == 0 ? "success" : "failed");
}

static void session_remove_service(ElaCarrier *w, int argc, char *argv[])
{
    if (argc != 2) {
        output("Invalid command syntax.\n");
        return;
    }

    ela_session_remove_service(session_ctx.ws, argv[1]);
    output("Service %s removed.", argv[1]);
}

static void portforwarding_open(ElaCarrier *w, int argc, char *argv[])
{
    PortForwardingProtocol protocol;
    int pfid;

    if (argc != 6) {
        output("Invalid command syntax.\n");
        return;
    }

    if (strcmp(argv[3], "tcp") == 0)
        protocol = PortForwardingProtocol_TCP;
    else {
        output("Unknown protocol %s.\n", argv[3]);
        return;
    }

    pfid = ela_stream_open_port_forwarding(session_ctx.ws, atoi(argv[1]),
                                argv[2], protocol, argv[4], argv[5]);

    if (pfid > 0) {
        output("Open portforwarding to service %s <<== %s:%s success, id is %d.\n",
               argv[2], argv[4], argv[5], pfid);
    } else {
        output("Open portforwarding to service %s <<== %s:%s failed.\n",
               argv[2], argv[4], argv[5]);
    }
}

static void portforwarding_close(ElaCarrier *w, int argc, char *argv[])
{
    int pfid;

    if (argc != 3) {
        output("Invalid command syntax.\n");
        return;
    }

    pfid = atoi(argv[2]);
    if (pfid <= 0) {
        output("Invalid portforwarding id %s.\n", argv[2]);
        return;
    }

    pfid = ela_stream_close_port_forwarding(session_ctx.ws, atoi(argv[1]), pfid);
    output("Portforwarding %d closed.\n", pfid);
}

static void help(ElaCarrier *w, int argc, char *argv[]);

//
static void online_test(ElaCarrier *w, int argc, char *argv[])
{
    if (argc != 2) {
        output("Invalid command syntax.\n");
        return;
    }

    test_loopcnt = atoi(argv[1]);
    if (test_loopcnt < 0) test_loopcnt = 1;

    bQuit = false;
    bOnlineTest = true;
    ela_kill(w);
}

struct session_test_args {
    ElaCarrier* w;
    int type;
    char userid[ELA_MAX_ID_LEN + 1];
};
struct session_test_args sessiontest_args;

static void *session_test_thread(void *arg)
{
    int i;
    struct session_test_args *args = (struct session_test_args *)arg;

    output("Begin session_test_thread");

    int packets = 1000, packetSize = 2048, failCnt = 0, totalFail = 0;
    gSessionSendBytes = packets * packetSize;
    char args_sessiontest[64];
    sprintf(args_sessiontest, "sessiontest %d %d", packetSize, packets);
    char *args_sendmsg[4] = {"", args->userid, args_sessiontest};

    struct timeval start_test, end_test;
    gettimeofday(&start_test, NULL);

    float consumeTime;
    for (i = 0; i < test_loopcnt; i++) {
        if (carrier_ctx.status != ElaConnectionStatus_Connected) {
            output(" wair for ElaStreamState_connected\n");
            cond_wait(&carrier_cond);
        }
        gettimeofday(&end_test, NULL);
        consumeTime = CalConsumeTime(end_test, start_test);
        output(" ---<<< loop:%d consume:%f>>>---\n", i, consumeTime);

        session_ctx.ws = ela_session_new(args->w, args->userid);
        if (!session_ctx.ws) {
            output("Create session failed. userid:%s\n", args->userid);
        } else {
            output("Create session successfully.\n");
        }
        session_ctx.unchanged_streams = 0;

        //
        int streamId = stream_addByOption(args->w, 4);
        if (-1 == streamId) {
            totalFail++;
            output("stream_add failed.\n");
            ela_session_close(session_ctx.ws);
            session_ctx.ws = NULL;
            continue;
        }
        output("Line %d.streamId = %d\n", __LINE__, streamId);
        output("wait stream initialized sem_wait\n");
        sem_wait(&session_status);

        if (streamState >= ElaStreamState_connected) {
            totalFail++;
            output("streamState error\n");
            ela_session_remove_stream(session_ctx.ws, streamId);
            ela_session_close(session_ctx.ws);
            continue;
        }

        session_request(args->w, 1, NULL);
        output("sem_wait\n");
        sem_wait(&session_status);
        output("Line %d.\n", __LINE__);

        //
        // stream_bulk_write(args->w, 4, args_senddata);
        send_message(args->w, 3, args_sendmsg);
        output("sem_wait\n");
        sem_wait(&session_status);

        // sleep(1);
        ela_session_remove_stream(session_ctx.ws, streamId);
        // sleep(1);
        ela_session_close(session_ctx.ws);
        // sem_wait(&session_status);
        sleep(2);

        if (streamState != ElaStreamState_closed) {
            totalFail++;
            if (++failCnt >= 3) {
                output("stream state is wrong! stop test\n");
                break;
            }
        }
        else {
            failCnt = 0;
        }
        //todo reset all session_ctx?
        session_ctx.ws = NULL;
        session_ctx.remote_sdp[0] = 0;
        session_ctx.sdp_len = 0;
    }

    gettimeofday(&end_test, NULL);
    consumeTime = CalConsumeTime(end_test, start_test);
    output("\nFinish! session_test_thread, totalTime:%f\n", consumeTime);
    OutputData(&testDataSessionSpeed, "sessionSpeed.txt");
    bSessionTest = false;

    return NULL;
}


static void session_test(ElaCarrier *w, int argc, char *argv[])
{
    if (argc != 3) {
        output("Invalid command syntax.\n");
        return;
    }

    test_loopcnt = atoi(argv[2]);
    if (test_loopcnt < 0) test_loopcnt = 1;
    bSessionTest = true;

    sessiontest_args.w = w;
    strcpy(sessiontest_args.userid, argv[1]);

    pthread_attr_t attr;
    pthread_t th;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&th, &attr, session_test_thread, &sessiontest_args);
    pthread_attr_destroy(&attr);
}

static void createsession_test(ElaCarrier *w, int argc, char *argv[])
{
    if (argc != 3) {
        output("Invalid command syntax.\n");
        return;
    }

    test_loopcnt = atoi(argv[2]);
    if (test_loopcnt < 0) test_loopcnt = 1;

    int i;
    for (i = 0; i < test_loopcnt; i++) {
        output(" ---<<< loop:%d >>>---\n", i);

        session_ctx.ws = ela_session_new(w, argv[1]);
        if (!session_ctx.ws) {
            output("Create session failed. userid:%s\n", argv[1]);
        } else {
            output("Create session successfully.\n");
        }
        ela_session_close(session_ctx.ws);
    }
}

static void testCreateStream(ElaCarrier *w, char * friendid, int loop)
{
    session_ctx.ws = ela_session_new(w, friendid);
    if (!session_ctx.ws) {
        output("Create session failed. userid:%s\n", friendid);
    } else {
        output("Create session successfully.\n");
    }

    int i;
    for (i = 0; i < loop; i++) {
        output(" ---<<< loop:%d >>>---\n", i);

        int streamId = stream_addByOption(w, 4);
        if (-1 == streamId) {
            output("stream_add failed.\n");
            ela_session_close(session_ctx.ws);
            session_ctx.ws = NULL;
            continue;
        }
        output("wait stream initialized sem_wait\n");
        sem_wait(&session_status);

        sleep(1);//?
        ela_session_remove_stream(session_ctx.ws, streamId);
        sleep(1);//?
        ela_session_close(session_ctx.ws);
        // sem_wait(&session_status);
        sleep(2);//?
    }
    ela_session_close(session_ctx.ws);
}

static void createstream_test(ElaCarrier *w, int argc, char *argv[])
{
    if (argc != 4) {
        output("Invalid command syntax.\n");
        return;
    }

    test_loopcnt = atoi(argv[2]);
    if (test_loopcnt < 0) test_loopcnt = 1;

    testCreateStream(w, argv[1], test_loopcnt);
}

struct addfriend_test_args {
    ElaCarrier* w;
    char addressID[ELA_MAX_ADDRESS_LEN + 1];
};
struct addfriend_test_args addfriendtest_args;

static void addfriend_test_thread(void *arg)
{
    struct addfriend_test_args *args = (struct addfriendtest_args *)arg;

    char *friendAddress;

    friendAddress = args->addressID;
    if (!ela_address_is_valid(friendAddress)) {
        output("address is Invalid. %s\n", friendAddress);
        return;
    }

    struct timeval startTime, endTime;
    gettimeofday(&startTime, NULL);

    ela_get_id_by_address(friendAddress, testFriendId, ELA_MAX_ID_LEN + 1);
    output("testFriendId :%s\n", testFriendId);

    char *removeArgs[2] = {"", testFriendId};
    char *addArgs[3] = {"", friendAddress, "auto-reply"};
    char *msgArgs[3] = {"", testFriendId, "remove"};

    bAddFriendTest = true;

    int i, rc, wait_times, timeuse;
    for (i = 0; i < test_loopcnt; i++) {
        output("loop :%d\n", i);
        if (ela_is_friend(args->w, testFriendId)) {
            wait_times = 0;
            do {
                rc = send_message(args->w, 3, msgArgs);
                sleep(1);
            } while ((0!=rc) && (wait_times++ < 60));
            friend_remove(args->w, 2, removeArgs);
            sleep(3);
        }
        //wait for callback
        gettimeofday(&addFriendTime.start_stamp, NULL);
        friend_add(args->w, 3, addArgs);
        output("wait for callback (cond_wait)\n");
        cond_wait(&friendOnLine_cond);
        gettimeofday(&addFriendTime.end_stamp, NULL);
        timeuse = 1000 * (addFriendTime.end_stamp.tv_sec -addFriendTime.start_stamp.tv_sec) + (addFriendTime.end_stamp.tv_usec - addFriendTime.start_stamp.tv_usec) / 1000;
        output("addfrind: %d ms\n", timeuse);
        addData(&testDataAddFriend, timeuse);
    }

    testFriendId[0] = 0;
    bAddFriendTest = false;

    OutputData(&testDataAddFriend, "testDataAddFriend.txt");
    Dispose(&testDataAddFriend);

    gettimeofday(&endTime, NULL);
    timeuse = endTime.tv_sec -startTime.tv_sec;
    output("addfriend_test End, ElapseTime: %ds\n", timeuse);
}

static void addfriend_test(ElaCarrier *w, int argc, char *argv[])
{
    if (argc != 3) {
        output("Invalid command syntax.\n");
        return;
    }

    test_loopcnt = atoi(argv[2]);
    if (test_loopcnt < 0) test_loopcnt = 1;
    bAddFriendTest = true;

    addfriendtest_args.w = w;
    strcpy(addfriendtest_args.addressID, argv[1]);

    pthread_attr_t attr;
    pthread_t th;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&th, &attr, addfriend_test_thread, &addfriendtest_args);
    pthread_attr_destroy(&attr);
}

struct command {
    const char *cmd;
    void (*function)(ElaCarrier *w, int argc, char *argv[]);
    const char *help;
} commands[] = {
    { "help",       help,                   "help [cmd]" },
    { "clear",      clear_screen,           "clear [log | out]" },

    { "address",    get_address,            "address" },
    { "nodeid",     get_nodeid,             "nodeid" },
    { "userid",     get_userid,             "userid" },
    { "me",         self_info,              "me [set] [name | description | gender | phone | email | region] [value]" },
    { "nospam",     self_nospam,            "nospam [ value ]" },
    { "presence",   self_presence,          "presence [ none | away | busy ]" },

    { "fadd",       friend_add,             "fadd address hello" },
    { "faccept",    friend_accept,          "faccept userid" },
    { "fremove",    friend_remove,          "fremove userid" },
    { "friends",    list_friends,           "friends" },
    { "friend",     show_friend,            "friend userid" },
    { "label",      label_friend,           "label userid name" },
    { "msg",        send_message,           "msg userid message" },
    { "invite",     invite,                 "invite userid data" },
    { "ireply",     reply_invite,           "ireply userid [confirm message | refuse reason]" },

    { "sinit",      session_init,           "sinit" },
    { "snew",       session_new,            "snew userid" },
    { "sadd",       stream_add,             "sadd [plain] [reliable] [multiplexing] [portforwarding]"},
    { "sremove",    stream_remove,          "sremove id" },
    { "srequest",   session_request,        "srequest" },
    { "sreply",     session_reply_request,  "sreply ok/sreply refuse [reason]"},
    { "swrite",     stream_write,           "swrite streamid string" },
    { "sbulkwrite", stream_bulk_write,      "sbulkwrite streamid packet-size packet-count" },
    { "sbulkrecv",  stream_bulk_receive,    "sbulkrecv start | end" },
    { "scadd",      stream_add_channel,     "scadd stream" },
    { "sinfo",      stream_get_info,        "sinfo id"},
    { "scclose",    stream_close_channel,   "scclose stream channel" },
    { "scwrite",    stream_write_channel,   "scwrite stream channel string" },
    { "scpend",     stream_pend_channel,    "scpend stream channel" },
    { "scresume",   stream_resume_channel,  "scresume stream channel" },
    { "sclose",     session_close,          "sclose" },
    { "spfsvcadd",  session_add_service,    "spfsvcadd name tcp|udp host port" },
    { "spfsvcremove", session_remove_service, "spfsvcremove name" },
    { "spfopen",    portforwarding_open,    "spfopen stream service tcp|udp host port" },
    { "spfclose",   portforwarding_close,   "spfclose stream pfid" },
    { "scleanup",   session_cleanup,        "scleanup" },
    { "kill",       kill_carrier,           "kill" },
    { "onlinetest", online_test,            "onlinetest loopcount" },
    { "sessiontest", session_test,          "sessiontest userid loopcount" },
    { "addfriendtest", addfriend_test,      "addfriendtest address loopcount" },
    { "csstress",   createsession_test,     "csstress userid loopcount"},
    { NULL }
};

static void help(ElaCarrier *w, int argc, char *argv[])
{
    char line[256] = "\x0";
    size_t len = 0;
    size_t cmd_len;
    struct command *p;

    if (argc == 1) {
        output("Available commands list:\n");

        for (p = commands; p->cmd; p++) {
            cmd_len = strlen(p->cmd);
            if (len + cmd_len + 1 > OUTPUT_COLS - 2) {
                output("  %s\n", line);
                strcpy(line, p->cmd);
                strcat(line, " ");
                len = cmd_len + 1;
            } else {
                strcat(line, p->cmd);
                strcat(line, " ");
                len += cmd_len + 1;
            }
        }

        if (len > 0)
            output("  %s\n", line);
    } else {
        for (p = commands; p->cmd; p++) {
            if (strcmp(argv[1], p->cmd) == 0) {
                output("Usage: %s\n", p->help);
                return;
            }
        }

        output("Unknown command: %s\n", argv[1]);
    }
}

static void do_cmd(ElaCarrier *w, char *line)
{
    char *args[512];
    int count = 0;
    char *p;
    int word = 0;

    for (p = line; *p != 0; p++) {
        if (isspace(*p)) {
            *p = 0;
            word = 0;
        } else {
            if (word == 0) {
                args[count] = p;
                count++;
            }

            word = 1;
        }
    }

    if (count > 0) {
        struct command *p;

        for (p = commands; p->cmd; p++) {
            if (strcmp(args[0], p->cmd) == 0) {
                p->function(w, count, args);
                return;
            }
        }

        output("Unknown command: %s\n", args[0]);
    }
}

static void idle_callback(ElaCarrier *w, void *context)
{
    char *cmd = read_cmd();

    if (cmd) do_cmd(w, cmd);
}

static void connection_callback(ElaCarrier *w, ElaConnectionStatus status,
                                void *context)
{
    switch (status) {
    case ElaConnectionStatus_Connected:
        gettimeofday(&friendOnline.middle_stamp, NULL);
        int timeuse = 1000 * (friendOnline.middle_stamp.tv_sec -friendOnline.start_stamp.tv_sec) + (friendOnline.middle_stamp.tv_usec - friendOnline.start_stamp.tv_usec) / 1000;
        addData(&testDataOnLine, timeuse);
        output("Connected to carrier network: %d ms\n",timeuse);
        cond_signal(&carrier_cond);
        break;

    case ElaConnectionStatus_Disconnected:
        output("Disconnect from carrier network.\n");
        gettimeofday(&friendOnline.start_stamp, NULL);
        cond_reset(&carrier_cond);
        break;

    default:
        output("Error!!! Got unknown connection status %d.\n", status);
    }

    carrier_ctx.status = status;
}

static void friend_info_callback(ElaCarrier *w, const char *friendid,
                                 const ElaFriendInfo *info, void *context)
{
    output("Friend information changed:\n");
    display_friend_info(info);
}

static void friend_connection_callback(ElaCarrier *w, const char *friendid,
                                       ElaConnectionStatus status, void *context)
{
    switch (status) {
    case ElaConnectionStatus_Connected:

        gettimeofday(&friendOnline.end_stamp, NULL);
        int timeuse = 1000 * (friendOnline.end_stamp.tv_sec -friendOnline.middle_stamp.tv_sec) + (friendOnline.end_stamp.tv_usec - friendOnline.middle_stamp.tv_usec) / 1000;
        output("Friend[%s] connection changed to be online %d ms\n", friendid, timeuse);
        addData(&testDataFriendOnLine, timeuse);
        if (bOnlineTest) {
            if (loopcount >= test_loopcnt) {
                OutputData(&testDataOnLine, "testDataOnLine.txt");
                OutputData(&testDataFriendOnLine, "testDataFriendOnLine.txt");
                Dispose(&testDataOnLine);
                Dispose(&testDataFriendOnLine);
                bOnlineTest = false;
            }
            ela_kill(w);
        }

        if (bAddFriendTest && (0 == strcmp(friendid, testFriendId))) {
            output("friend_connection_callback cond_signal\n");
            cond_signal(&friendOnLine_cond);
        }


        //---------
        // testCreateStream(w, friendid, 100);

        break;

    case ElaConnectionStatus_Disconnected:
        output("Friend[%s] connection changed to be offline.\n", friendid);
        break;

    default:
        output("Error!!! Got unknown connection status %d.\n", status);
    }
}

static void friend_presence_callback(ElaCarrier *w, const char *friendid,
                                     ElaPresenceStatus status,
                                     void *context)
{
    if (status >= ElaPresenceStatus_None &&
        status <= ElaPresenceStatus_Busy) {
        output("Friend[%s] change presence to %s\n", friendid, presence_name[status]);
    } else {
        output("Error!!! Got unknown presence status %d.\n", status);
    }
}

static void friend_request_callback(ElaCarrier *w, const char *userid,
                                    const ElaUserInfo *info, const char *hello,
                                    void *context)
{
    output("Friend request from user[%s] with HELLO: %s.\n",
           *info->name ? info->name : userid, hello);
    output("Reply use following commands:\n");
    output("  faccept %s\n", userid);
}

static void message_callback(ElaCarrier *w, const char *from,
                             const char *msg, size_t len, void *context)
{
    output("Message from friend[%s]: %.*s\n", from, (int)len, msg);
    //parsemsg
}

static void invite_request_callback(ElaCarrier *w, const char *from,
                                    const char *data, size_t len, void *context)
{
    output("Invite request from[%s] with data: %.*s\n", from, (int)len, data);
    output("Reply use following commands:\n");
    output("  ireply %s confirm [message]\n", from);
    output("  ireply %s refuse [reason]\n", from);
}

static void usage(void)
{
    printf("Elastos shell, an interactive console client application.\n");
    printf("Usage: elashell [OPTION]...\n");
    printf("\n");
    printf("First run options:\n");
    printf("  -c, --config=CONFIG_FILE  Set config file path.\n");
    printf("\n");
    printf("Debugging options:\n");
    printf("      --debug               Wait for debugger attach after start.\n");
    printf("\n");
}

#include <sys/resource.h>

int sys_coredump_set(bool enable)
{
    const struct rlimit rlim = {
        enable ? RLIM_INFINITY : 0,
        enable ? RLIM_INFINITY : 0
    };

    return setrlimit(RLIMIT_CORE, &rlim);
}

void signal_handler(int signum)
{
    output("signal_handler signum:%d\n", signum);
    getchar();
    cleanup_screen();
    history_save();
    exit(-1);
}


int main(int argc, char *argv[])
{
    ShellConfig *cfg;
    char buffer[2048] = {0};
    ElaCarrier *w;
    ElaOptions opts;
    int wait_for_attach = 0;
    char buf[ELA_MAX_ADDRESS_LEN+1];
    ElaCallbacks callbacks;
    int rc;
    int i;

    int opt;
    int idx;
    struct option options[] = {
        { "config",         required_argument,  NULL, 'c' },
        { "debug",          no_argument,        NULL, 2 },
        { "help",           no_argument,        NULL, 'h' },
        { NULL,             0,                  NULL, 0 }
    };

    signal(SIGINT, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGSEGV, signal_handler);

    sys_coredump_set(true);

    memset(&opts, 0, sizeof(opts));

    while ((opt = getopt_long(argc, argv, "c:h?",
            options, &idx)) != -1) {
        switch (opt) {
        case 'c':
            strcpy(buffer, optarg);
            break;

        case 2:
            wait_for_attach = 1;
            break;

        case 'h':
        case '?':
        default:
            usage();
            exit(-1);
        }
    }

    if (wait_for_attach) {
        printf("Wait for debugger attaching, process id is: %d.\n", getpid());
        printf("After debugger attached, press any key to continue......");
        getchar();
    }

    if (!*buffer) {
        realpath(argv[0], buffer);
        strcat(buffer, ".conf");
    }

    cfg = load_config(buffer);
    if (!cfg) {
        fprintf(stderr, "loading configure failed !\n");
        return -1;
    }

    init_screen();
    history_load();

    ela_log_init(cfg->loglevel, cfg->logfile, log_print);

    opts.udp_enabled = cfg->udp_enabled;
    opts.persistent_location = cfg->datadir;
    opts.bootstraps_size = cfg->bootstraps_size;
    opts.bootstraps = (BootstrapNode *)calloc(1, sizeof(BootstrapNode) * opts.bootstraps_size);
    if (!opts.bootstraps) {
        fprintf(stderr, "out of memory.");
        deref(cfg);
        return -1;
    }

    for (i = 0 ; i < cfg->bootstraps_size; i++) {
        BootstrapNode *b = &opts.bootstraps[i];
        BootstrapNode *node = cfg->bootstraps[i];

        b->ipv4 = node->ipv4;
        b->ipv6 = node->ipv6;
        b->port = node->port;
        b->public_key = node->public_key;
    }

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.idle = idle_callback;
    callbacks.connection_status = connection_callback;
    callbacks.friend_list = friends_list_callback;
    callbacks.friend_connection = friend_connection_callback;
    callbacks.friend_info = friend_info_callback;
    callbacks.friend_presence = friend_presence_callback;
    callbacks.friend_request = friend_request_callback;
    callbacks.friend_added = friend_added_callback;
    callbacks.friend_removed = friend_removed_callback;
    callbacks.friend_message = message_callback;
    callbacks.friend_invite = invite_request_callback;

    do {
        w = ela_new(&opts, &callbacks, NULL);
        gettimeofday(&friendOnline.start_stamp, NULL);

        if (!w) {
            output("Error create carrier instance: 0x%x\n", ela_get_error());
            output("Press any key to quit...");
            nodelay(stdscr, false);
            getch();
            goto quit;
        }

        output("Carrier node identities:\n");
        output("   Node ID: %s\n", ela_get_nodeid(w, buf, sizeof(buf)));
        output("   User ID: %s\n", ela_get_userid(w, buf, sizeof(buf)));
        output("   Address: %s\n\n", ela_get_address(w, buf, sizeof(buf)));
        output("\n");

        session_init(w, 1, NULL);

        bQuit = true;
        carrier_ctx.status = ElaConnectionStatus_Disconnected;
        rc = ela_run(w, 10);
        if (rc != 0) {
            output("Error start carrier loop: 0x%x\n", ela_get_error());
            output("Press any key to quit...");
            nodelay(stdscr, false);
            getch();
            ela_kill(w);
            goto quit;
        }
        output("loopcount: %d\n", loopcount);
        if (loopcount++ > test_loopcnt) {
            loopcount = 0;
            bQuit = true;
        }
    } while (!bQuit);

quit:
    ela_session_cleanup(w);
    deref(cfg);
    free(opts.bootstraps);

    cleanup_screen();
    history_save();
    // sleep(2);
    return 0;
}

