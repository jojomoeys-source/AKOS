#include "independent_worker.h"

int main(void) {
    return runIndependentWorker(INDEPENDENT_DISPATCHER, 0, "elevator.log") == 0
        ? 0
        : 1;
}
