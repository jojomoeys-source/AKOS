#include "independent_worker.h"
#include "shared_state.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argumentCount, char **argumentValues) {
    if (argumentCount != 2) {
        fprintf(stderr, "Usage: %s PERSON_INDEX\n", argumentValues[0]);
        return 1;
    }

    char *end = NULL;
    errno = 0;
    long index = strtol(argumentValues[1], &end, 10);
    if (errno != 0 || end == argumentValues[1] || *end != '\0' ||
        index < 0 || index >= MAX_PASSENGERS) {
        fprintf(stderr, "Invalid passenger index\n");
        return 1;
    }

    return runIndependentWorker(
        INDEPENDENT_PERSON,
        (int)index,
        "elevator.log"
    ) == 0 ? 0 : 1;
}
