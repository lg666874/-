#ifndef FFMSG_QUEUE_H
#define FFMSG_QUEUE_H

#include "SDL.h"

typedef struct AVMessage {
    int what;
    int arg1;
    int arg2;
    void *obj;
    void (*free_l)(void *obj);
    struct AVMessage *next;
} AVMessage;

typedef struct MessageQueue {
    AVMessage *first_msg, *last_msg;
    int nb_messages;
    int abort_request;
    SDL_mutex *mutex;
    SDL_cond *cond;

    AVMessage *recycle_msg;
    int recycle_count;
    int alloc_count;
} MessageQueue;





void msg_free_res(AVMessage *msg);


int msg_queue_put_private(MessageQueue *q, AVMessage *msg);

int msg_queue_put(MessageQueue *q, AVMessage *msg);

void msg_init_msg(AVMessage *msg);


void msg_queue_put_simple1(MessageQueue *q, int what);


void msg_queue_put_simple2(MessageQueue *q, int what, int arg1);


void msg_queue_put_simple3(MessageQueue *q, int what, int arg1, int arg2);


void msg_obj_free_l(void *obj);

void msg_queue_put_simple4(MessageQueue *q, int what, int arg1, int arg2, void *obj, int obj_len);


void msg_queue_init(MessageQueue *q);

void msg_queue_flush(MessageQueue *q);


void msg_queue_destroy(MessageQueue *q);


void msg_queue_abort(MessageQueue *q);

void msg_queue_start(MessageQueue *q);


/* return < 0 if aborted, 0 if no msg and > 0 if msg.  */
int msg_queue_get(MessageQueue *q, AVMessage *msg, int block);


void msg_queue_remove(MessageQueue *q, int what);

#endif // FFMSG_QUEUE_H
