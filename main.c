#include <stdlib.h>
#include <zookeeper/zookeeper.h>

#ifdef THREADED
#pragma message "mt build"
#else
#pragma message "st build"
#endif

int main(void)
{
    zookeeper_close(NULL);
    return EXIT_SUCCESS;
}
