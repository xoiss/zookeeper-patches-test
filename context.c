#ifdef THREADED
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#endif
#include <string.h>
#include "context.h"

void context_init(struct context_s *context)
{
    memset(context, 0, sizeof(*context));

#ifdef THREADED
    int rc = pthread_mutex_init(&context->mutex, NULL);
    assert(rc == 0);
#endif
}

void context_lock(struct context_s *context)
{
#ifdef THREADED
    int rc = pthread_mutex_lock(&context->mutex);
    if (rc != 0) {
        perror("pthread_mutex_lock");
        abort();
    }
#else
    (void)context;
#endif
}

void context_unlock(struct context_s *context)
{
#ifdef THREADED
    int rc = pthread_mutex_unlock(&context->mutex);
    if (rc != 0) {
        perror("pthread_mutex_lock");
        abort();
    }
#else
    (void)context;
#endif
}

enum state_e context_get_state(struct context_s *context)
{
    enum state_e state;

    CONTEXT_ATOMIC(context, {
        state = context->state;
    });

    return state;
}

void context_set_state(struct context_s *context, enum state_e state)
{
    unsigned apply = 0;

    CONTEXT_ATOMIC(context, {
        enum state_e prev_state = context->state;
        switch (state) {
        case CONTEXT_INACTIVE:
            apply = 1;
            break;
        case CONTEXT_ACTIVE:
            apply = (prev_state == CONTEXT_INACTIVE);
            break;
        case CONTEXT_SUSPEND:
            apply = (prev_state == CONTEXT_ACTIVE);
            break;
        case CONTEXT_INTERRUPT:
            apply = (prev_state == CONTEXT_ACTIVE);
            break;
        }
        if (apply) {
            context->state = state;
        }
    });
}

zhandle_t* context_get_zhandle(struct context_s *context)
{
    zhandle_t *zhandle;

    CONTEXT_ATOMIC(context, {
        zhandle = context->zhandle;
    });

    return zhandle;
}

void context_set_zhandle(struct context_s *context, zhandle_t *zhandle)
{
    CONTEXT_ATOMIC(context, {
        context->zhandle = zhandle;
    });
}
