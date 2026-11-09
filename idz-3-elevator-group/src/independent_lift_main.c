#include "independent_worker.h"
#include "shared_state.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argumentCount, char **argumentValues) {
    if (argumentCount != 2) {
        fprintf(stderr, "Usage: %s LIFT_INDEX\n", argumentValues[0]);
        return 1;
    }

    char *end = NULL;
    errno = 0;
    long index = strtol(argumentValues[1], &end, 10);
    if (errno != 0 || end == argumentValues[1] || *end != '\0' ||
        index < 0 || index >= MAX_LIFTS) {
        fprintf(stderr, "Invalid lift index\n");
        return 1;
    }

    return runIndependentWorker(
        INDEPENDENT_LIFT,
        (int)index,
        "elevator.log"
    ) == 0 ? 0 : 1;
}
