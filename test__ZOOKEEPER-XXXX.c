#include <assert.h>
#include <stddef.h>
#include <zookeeper/zookeeper.h>

static void step_0();
void test__ZOOKEEPER_XXXX(const char *zookeeper_host,
        zhandle_t **zhandle, volatile unsigned *active)
{
    step_0(zookeeper_host, zhandle, active);
}

static void step_0_complete();
static void step_0(const char *zookeeper_host,
        zhandle_t **zhandle, volatile unsigned *active)
{
    fprintf(stderr, "-- %s\n", __FUNCTION__);

    *active = 1;

    *zhandle = zookeeper_init(
            zookeeper_host, step_0_complete, /*timeout, ms*/ 10000,
            /*client_id*/ NULL, /*context*/ (void*)active, /*flags*/ 0);
    assert(zhandle != NULL);
}

static void step_0_complete(
        zhandle_t *zhandle, int type, int state,
        const char *path, void *context)
{
    fprintf(stderr, "-- %s\n", __FUNCTION__);

    (void)path;

    volatile unsigned *active = context;
    if (!*active || type != ZOO_SESSION_EVENT || state != ZOO_CONNECTED_STATE) {
        return;
    }

    assert(zhandle != NULL);
    zoo_set_watcher(zhandle, NULL);

    zookeeper_close(zhandle);
    *active = 0;
}
