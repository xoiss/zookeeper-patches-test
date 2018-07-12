#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <zookeeper/zookeeper.h>

static void zookeeper_cycle(zhandle_t *zhandle, const volatile unsigned *active)
{
    while (*active) {

#ifndef THREADED

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
            exit(EXIT_FAILURE);
        }

        if (rc) {
            int events =
                    (FD_ISSET(fd, &rfds) ? ZOOKEEPER_READ : 0) |
                    (FD_ISSET(fd, &wfds) ? ZOOKEEPER_WRITE : 0);
            rc = zookeeper_process(zhandle, events);
            assert(rc == ZOK);
        }

#else
        (void)zhandle;
        struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 };
        if (select(0, NULL, NULL, NULL, &tv) == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }

#endif

    }
}

#define ZOOKEEPER_HOST "localhost:2181"

extern void test__ZOOKEEPER_XXXX();

int main(void)
{
    zhandle_t *zhandle;
    static volatile unsigned active;

    test__ZOOKEEPER_XXXX(ZOOKEEPER_HOST, &zhandle, &active);
    zookeeper_cycle(zhandle, &active);

    fprintf(stderr, "ok\n");
    return EXIT_SUCCESS;
}
