/*  https://issues.apache.org/jira/browse/ZOOKEEPER-2894
 *
 *  ZooKeeper C Client single thread build
 *
 *  First of all, ZooKeeper C Client design allows calling zookeeper_close() in
 *  two ways:
 *  a) from a ZooKeeper callback handler (completion or watcher) which in turn
 *      is called through zookeeper_process()
 *  b) and from other places -- i.e., when the call-stack does not pass through
 *      any of zookeeper mechanics prior to enter into mentioned
 *      zookeeper_close()
 *
 *  The issue described here below is specific only to the case (b). So, it's Ok
 *  with the case (a).
 *
 *  When zookeeper_close() is called in the (b) way, the following happens:
 *  1. If there are requests waiting for responses in zh.sent_requests queue,
 *      they all are removed from this queue and each of them is "completed"
 *      with personal fake response having status ZCLOSING. Such fake responses
 *      are put into zh.completions_to_process queue. It's Ok
 *  2. But then, zh.completions_to_process queue is left unhandled. Neither
 *      completion callbacks are called, nor dynamic memory allocated for fake
 *      responses is freed
 *  3. Different structures within zh are dismissed and finally zh is freed
 *
 *  This is illustrated on the screenshot attached to this ticket: you may see
 *  that the next instruction to execute will be free(zh) while
 *  zh.completions_to_process queue is not empty (see the "Variables" tab to the
 *  right).
 *
 *  Alternatively, the same situation but in the case (a) is handled properly --
 *  i.e., all completion callback handlers are truly called with ZCLOSING and
 *  the memory is freed, both for subcases (a.1) when there is a failure like
 *  connection-timeout, connection-closed, etc., or (a.2) there is not failure.
 *  The reason is that any callback handler (completion or watcher) in the case
 *  (a) is called from the process_completions() function which runs in the loop
 *  until zh.completions_to_process queue gets empty. So, this function
 *  guarantees this queue to be completely processed even if new completions
 *  occur during reaction on previously queued completions.
 *
 *  Consequently:
 *  1. At least there is definitely the memory leak in the case (b) -- all the
 *      fake responses put into zh.completions_to_process queue are lost after
 *      free(zh)
 *  2. And it looks like a great misbehavior not to call completions on sent
 *      requests in the case (b) while they are called with ZCLOSING in the case
 *      (a) -- so, I think it's not "by design" but a completions leak
 *
 *  To reproduce the case (b) do the following:
 *  - open ZooKeeper session, connect to a server, receive and process
 *      connected-watch, etc.
 *  - then somewhere from the main events loop call for example zoo_acreate()
 *      with valid arguments -- it shall return ZOK
 *  - then, immediately after it returned, call zookeeper_close()
 *  - note that completion callback handler for zoo_acreate() will not be called
 *
 *  To reproduce the case (a) do the following:
 *  - the same as above, open ZooKeeper session, connect to a server, receive
 *      and process connected-watch, etc.
 *  - the same as above, somewhere from the main events loop call for example
 *      zoo_acreate() with valid arguments -- it shall return ZOK
 *  - but now don't call zookeeper_close() immediately -- wait for completion
 *      callback on the commenced request
 *  - when zoo_acreate() completes, from within its completion callback handler,
 *      call another zoo_acreate() and immediately after it returned call
 *      zookeeper_close()
 *  - note that completion callback handler for the second zoo_acreate() will be
 *      called with ZCLOSING, unlike the case (b) described above
 *
 *  To fix this I propose calling to process_completions() from
 *  destroy(zhandle_t *zh) as it is done in handle_error(zhandle_t *zh,int rc).
 *
 *  This is a proposed fix: https://github.com/apache/zookeeper/pull/363
 *
 *  [upd]
 *  There are another tickets with about the same problem: ZOOKEEPER-1493,
 *  ZOOKEEPER-2073 (the "same" just according to their titles). However, as I
 *  can see, the corresponding patches were applied on the branch 3.4.10, but
 *  the effect still persists -- so, this ticket does not duplicate the
 *  mentioned two.
 */

#include <assert.h>
#include <time.h>
#include <stdio.h>
#include <zookeeper/zookeeper.h>
#include "context.h"
#include "test__ZOOKEEPER-2894.h"

static void init_step_0();
extern void test__ZOOKEEPER_2894__init(struct context_s *context,
        const char *zookeeper_host)
{
    fprintf(stderr, "-- %s\n", __FUNCTION__);

    context_set_state(context, CONTEXT_ACTIVE);

    init_step_0(context, zookeeper_host);
}

static void init_step_0_complete();
static void init_step_0(struct context_s *context, const char *zookeeper_host)
{
    /* open ZooKeeper session, ...
     */

    fprintf(stderr, "-- %s\n", __FUNCTION__);

    zhandle_t *zhandle = zookeeper_init(
            zookeeper_host, init_step_0_complete, /*timeout, ms*/ 10000,
            /*client_id*/ NULL, (void*)context, /*flags*/ 0);
    assert(zhandle != NULL);

    context_set_zhandle(context, zhandle);
}

static void init_step_0_complete(
        zhandle_t *zhandle, int type, int state,
        const char *path, void *context)
{
    /* ... connect to a server, receive and process connected-watch, etc.
     */

    fprintf(stderr, "-- %s\n", __FUNCTION__);

    (void)path;

    if (context_get_state(context) != CONTEXT_ACTIVE
            || type != ZOO_SESSION_EVENT || state != ZOO_CONNECTED_STATE) {
        return;
    }

    assert(context_get_zhandle(context) == zhandle);

    zoo_set_watcher(zhandle, NULL);

    /* proceed the test from the main events loop */
    context_set_state(context, CONTEXT_INTERRUPT);
}

static void case_a_step_1();
void test__ZOOKEEPER_2894__case_a(struct context_s *context)
{
    fprintf(stderr, "-- %s\n", __FUNCTION__);

    context_set_state(context, CONTEXT_ACTIVE);

    case_a_step_1(context);
}

static void case_a_step_1_complete();
static void case_a_step_1(struct context_s *context)
{
    /* then somewhere from the main events loop call for example zoo_acreate()
     * with valid arguments -- it shall return ZOK ...
     */

    fprintf(stderr, "-- %s\n", __FUNCTION__);

    char path[100];
    snprintf(path, sizeof(path), "/%li", (long)time(/*timer*/ NULL));

    const char value[] = "some value";

    fprintf(stderr, "   + call zoo_acreate ('%s': '%s')\n", path, value);
    fprintf(stderr, "   + zoo_acreate is expected to complete with "
            "ZOK (%i)\n", ZOK);

    int rc = zoo_acreate(
            context_get_zhandle(context),
            path, value, sizeof(value) - 1,
            &ZOO_OPEN_ACL_UNSAFE, /*flags*/ 0,
            case_a_step_1_complete, /*context*/ (void*)context);
    assert(rc == ZOK);

    /* ... but don't call zookeeper_close() immediately -- wait for completion
     * callback on the commenced request
     */
}

static void case_a_step_2();
static void case_a_step_1_complete(int rc, const char *path,
        const void *context)
{
    /* when zoo_acreate() completes, from within its completion callback
     * handler ...
     */

    fprintf(stderr, "-- %s\n", __FUNCTION__);

    fprintf(stderr, "   + zoo_acreate completed: rc=%i, path='%s'\n", rc, path);

    assert(rc == ZOK);

    case_a_step_2((struct context_s*)context);
}

static void case_a_step_2_complete();
static void case_a_step_2(struct context_s *context)
{
    /* ... call another zoo_acreate() ...
     */

    fprintf(stderr, "-- %s\n", __FUNCTION__);

    char path[100];
    snprintf(path, sizeof(path), "/%li", (long)time(/*timer*/ NULL) + 1);

    const char value[] = "another value";

    fprintf(stderr, "   + call zoo_acreate ('%s': '%s')\n", path, value);
    fprintf(stderr, "   + and immediately call zookeeper_close\n");
    fprintf(stderr, "   + zoo_acreate is expected to complete with "
            "ZCLOSING (%i)\n", ZCLOSING);

    int rc = zoo_acreate(
            context_get_zhandle(context),
            path, value, sizeof(value) - 1,
            &ZOO_OPEN_ACL_UNSAFE, /*flags*/ 0,
            case_a_step_2_complete, /*context*/ (void*)context);
    assert(rc == ZOK);

    /* ... and immediately after it returned call zookeeper_close()
     */

    rc = zookeeper_close(context_get_zhandle(context));
    assert(rc == ZOK);

    context_set_state(context, CONTEXT_SUSPEND);
}

static void case_a_step_2_complete(int rc, const char *path,
        const void *context)
{
    /* note that completion callback handler for the second zoo_acreate() will
     * be called with ZCLOSING
     */

    fprintf(stderr, "-- %s\n", __FUNCTION__);

    fprintf(stderr, "   + zoo_acreate completed: rc=%i, path='%s'\n", rc, path);

    assert(rc == ZCLOSING);

    context_set_state((void*)context, CONTEXT_INACTIVE);
}

static void case_b_step_1();
void test__ZOOKEEPER_2894__case_b(struct context_s *context)
{
    fprintf(stderr, "-- %s\n", __FUNCTION__);

    context_set_state(context, CONTEXT_ACTIVE);

    case_b_step_1(context);
}

static void case_b_step_1_complete();
static void case_b_step_1(struct context_s *context)
{
    /* then somewhere from the main events loop call for example zoo_acreate()
     * with valid arguments -- it shall return ZOK ...
     */

    fprintf(stderr, "-- %s\n", __FUNCTION__);

    char path[100];
    snprintf(path, sizeof(path), "/%li", (long)time(/*timer*/ NULL));

    const char value[] = "some value";

    fprintf(stderr, "   + call zoo_acreate ('%s': '%s')\n", path, value);
    fprintf(stderr, "   + and immediately call zookeeper_close\n");

#ifdef THREADED
    fprintf(stderr, "   + in MT build: "
            "zoo_acreate is expected to complete with "
            "ZCLOSING (%i)\n", ZCLOSING);
#else
    fprintf(stderr, "   + in ST build with ZOOKEEPER-2894 patch: "
            "zoo_acreate is expected to complete with "
            "ZCLOSING (%i)\n", ZCLOSING);
    fprintf(stderr, "   + in ST build without ZOOKEEPER-2894 patch: "
            "zoo_acreate will not complete at all\n");
#endif

    int rc = zoo_acreate(
            context_get_zhandle(context),
            path, value, sizeof(value) - 1,
            &ZOO_OPEN_ACL_UNSAFE, /*flags*/ 0,
            case_b_step_1_complete, /*context*/ (void*)context);
    assert(rc == ZOK);

    /* ... then, immediately after it returned, call zookeeper_close()
     */

    rc = zookeeper_close(context_get_zhandle(context));
    assert(rc == ZOK);

    context_set_state(context, CONTEXT_SUSPEND);
}

static void case_b_step_1_complete(int rc, const char *path,
        const void *context)
{
    /* in MT build, and in ST build with ZOOKEEPER-2894 patch:
     *
     * note that completion callback handler for zoo_acreate() will be called
     * with ZCLOSING, as in the case (a)
     */

    /* in ST build without ZOOKEEPER-2894 patch:
     *
     * note that completion callback handler for zoo_acreate() will not be
     * called at all, unlike ST build in the case (a) and MT build in both cases
     */

    fprintf(stderr, "-- %s\n", __FUNCTION__);

    fprintf(stderr, "   + zoo_acreate completed: rc=%i, path='%s'\n", rc, path);

    assert(rc == ZCLOSING);

    context_set_state((void*)context, CONTEXT_INACTIVE);
}
