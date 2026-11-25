#ifndef SHARED_STATE_H
#define SHARED_STATE_H

#include <stddef.h>
#include <semaphore.h>

#include "call_queue.h"
#include "config.h"
#include "lift.h"
#include "person.h"

enum {
    MAX_PASSENGERS = 100,
    MAX_LIFTS = 10,
    MAX_CALLS = 100
};

typedef enum {
    SIMULATION_INITIALIZING,
    SIMULATION_RUNNING,
    SIMULATION_STOPPING,
    SIMULATION_FINISHED
} SimulationStatus;

typedef enum {
    STOP_REASON_NONE,
    STOP_REASON_ALL_DELIVERED,
    STOP_REASON_WORK_PERIOD,
    STOP_REASON_SIGINT,
    STOP_REASON_ERROR
} SimulationStopReason;

typedef struct {
    Call items[MAX_CALLS];
    int head;
    int tail;
    int count;
} SharedCallQueue;

typedef struct {
    int version;
    int initialized;

    SimulationStatus status;
    SimulationStopReason stopReason;
    int currentTime;
    int completedLiftCount;
    int expectedWorkerCount;
    int readyWorkerCount;
    int finishedWorkerCount;
    int workerErrorCount;

    sem_t processStateLock;
    sem_t processCallsAvailable;

    SimulationConfig config;
    Person people[MAX_PASSENGERS];
    Lift lifts[MAX_LIFTS];
    SharedCallQueue callQueue;
} SharedState;

SharedState *createSharedState(const char *name, int *fd, size_t *size);
SharedState *openSharedState(const char *name, int *fd, size_t *size);
void closeSharedState(SharedState *state, int fd, size_t size);
int removeSharedState(const char *name);

#endif // SHARED_STATE_H
