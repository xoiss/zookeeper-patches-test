#ifndef CONTEXT_H_
#define CONTEXT_H_

#ifdef THREADED
#include <pthread.h>
#endif

#include <zookeeper/zookeeper.h>

enum state_e {
    CONTEXT_INACTIVE = 0,
    CONTEXT_ACTIVE,
    CONTEXT_SUSPEND,
    CONTEXT_INTERRUPT,
};

struct context_s {
#ifdef THREADED
    pthread_mutex_t mutex;
#endif
    enum state_e state;
    zhandle_t *zhandle;
};

extern void context_init(struct context_s *context);
extern void context_lock(struct context_s *context);
extern void context_unlock(struct context_s *context);

#define CONTEXT_ATOMIC(context, code_block)\
    do {\
        context_lock(context);\
        do {\
            code_block;\
        } while(0);\
        context_unlock(context);\
    } while(0)

extern enum state_e context_get_state(struct context_s *context);
extern void context_set_state(struct context_s *context, enum state_e state);

extern zhandle_t* context_get_zhandle(struct context_s *context);
extern void context_set_zhandle(struct context_s *context, zhandle_t *zhandle);

#endif /* CONTEXT_H_ */
