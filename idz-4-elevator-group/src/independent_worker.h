#ifndef INDEPENDENT_WORKER_H
#define INDEPENDENT_WORKER_H

typedef enum {
    INDEPENDENT_PERSON,
    INDEPENDENT_DISPATCHER,
    INDEPENDENT_LIFT
} IndependentWorkerRole;

int runIndependentWorker(
    IndependentWorkerRole role,
    int index,
    const char *logPath
);

#endif
