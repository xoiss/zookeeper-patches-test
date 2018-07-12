#include <stdio.h>
#include <stdlib.h>
#include "context.h"
#include "cycle.h"

static struct context_s context;

#include "test__ZOOKEEPER-2894.h"

#define ZOOKEEPER_HOST "localhost:2181"

int main(void)
{
    context_init(&context);

    test__ZOOKEEPER_2894__init(&context, ZOOKEEPER_HOST);
    cycle_run(&context);
    test__ZOOKEEPER_2894__case_a(&context);
    cycle_run(&context);

    fprintf(stderr, "ok\n\n");

    test__ZOOKEEPER_2894__init(&context, ZOOKEEPER_HOST);
    cycle_run(&context);
    test__ZOOKEEPER_2894__case_b(&context);
    cycle_run(&context);

    fprintf(stderr, "ok\n");
    return EXIT_SUCCESS;
}
