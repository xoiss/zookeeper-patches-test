#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <time.h>
#include <zookeeper/zookeeper.h>
#include "context.h"
#include "cycle.h"

void cycle_run(struct context_s *context)
{
    for (;;) {

        if (context_get_state(context) == CONTEXT_INACTIVE) {
            return;
        }

#ifdef THREADED

        struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 };
        if (select(0, NULL, NULL, NULL, &tv) == -1) {
            perror("select");
            abort();
        }

#else

        if (context_get_state(context) == CONTEXT_SUSPEND) {
            context_set_state(context, CONTEXT_INACTIVE);
            return;
        }

        zhandle_t *zhandle = context_get_zhandle(context);
        assert(zhandle != NULL);

        int rc, fd, interest;
        struct timeval tv;

        rc = zookeeper_interest(zhandle, &fd, &interest, &tv);
        assert(rc == ZOK && fd >= 0 && fd < FD_SETSIZE);
        assert(interest & (ZOOKEEPER_READ | ZOOKEEPER_WRITE));

        fd_set rfds, wfds;

        FD_ZERO(&rfds);
        if (interest & ZOOKEEPER_READ) {
            FD_SET(fd, &rfds);
        }
        if (interest & ZOOKEEPER_WRITE) {
            FD_SET(fd, &wfds);
        }

        rc = select(fd + 1, &rfds, &wfds, NULL, &tv);
        if (rc == -1) {
            perror("select");
            abort();
        }

        if (rc) {
            int events =
                    (FD_ISSET(fd, &rfds) ? ZOOKEEPER_READ : 0) |
                    (FD_ISSET(fd, &wfds) ? ZOOKEEPER_WRITE : 0);
            rc = zookeeper_process(zhandle, events);
            assert(rc == ZOK);
        }

#endif

        if (context_get_state(context) == CONTEXT_INTERRUPT) {
            context_set_state(context, CONTEXT_INACTIVE);
            return;
        }
    }
}
