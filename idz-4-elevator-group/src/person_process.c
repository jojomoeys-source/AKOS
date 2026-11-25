#define _POSIX_C_SOURCE 200809L

#include "person_process.h"

#include "shared_queue.h"
#include "logger.h"

#include <errno.h>
#include <stdio.h>
#include <time.h>

static int lockSharedState(IpcSync *sync) {
    while (sem_wait(sync->stateLock) == -1) {
        if (errno == EINTR) {
            continue;
        }

        perror("sem_wait stateLock");
        return -1;
    }

    return 0;
}

static int unlockSharedState(IpcSync *sync) {
    if (sem_post(sync->stateLock) == -1) {
        perror("sem_post stateLock");
        return -1;
    }

    return 0;
}

static int waitOneSimulationTick(void) {
    struct timespec remaining = {
        .tv_sec = 0,
        .tv_nsec = 10 * 1000 * 1000
    };

    while (nanosleep(&remaining, &remaining) == -1) {
        if (errno == EINTR) {
            continue;
        }

        perror("nanosleep passenger arrival");
        return -1;
    }

    return 0;
}

static int readSimulationClock(
    SharedState *state,
    IpcSync *sync,
    int *currentTime
) {
    // Чтение модельного времени под блокировкой общей памяти
    if (lockSharedState(sync) == -1) {
        return -1;
    }

    int isRunning = state->status == SIMULATION_RUNNING;
    *currentTime = state->currentTime;

    if (unlockSharedState(sync) == -1) {
        return -1;
    }

    return isRunning ? 1 : 0;
}

int runPersonProcess(SharedState *state, IpcSync *sync, int personIndex) {
    // Ожидание времени появления и регистрация вызова пассажира
    if (state == NULL || sync == NULL ||
        sync->stateLock == SEM_FAILED ||
        sync->callsAvailable == SEM_FAILED ||
        personIndex < 0 ||
        personIndex >= state->config.peopleCount) {
        return -1;
    }

    if (lockSharedState(sync) == -1) {
        return -1;
    }

    if (state->status != SIMULATION_RUNNING) {
        unlockSharedState(sync);
        return 0;
    }

    int arrivalTime = state->people[personIndex].arrivalTime;
    int initialState = state->people[personIndex].state;

    if (unlockSharedState(sync) == -1) {
        return -1;
    }

    if (arrivalTime < 0) {
        return -1;
    }

    if (initialState != PERSON_NOT_ARRIVED) {
        return 0;
    }

    for (;;) {
        int currentTime;
        int isRunning = readSimulationClock(state, sync, &currentTime);
        if (isRunning == -1) {
            return -1;
        }
        if (!isRunning) {
            return 0;
        }
        if (currentTime >= arrivalTime) {
            break;
        }
        if (waitOneSimulationTick() == -1) {
            return -1;
        }
    }

    if (lockSharedState(sync) == -1) {
        return -1;
    }

    Person *person = &state->people[personIndex];
    if (state->status != SIMULATION_RUNNING ||
        person->state != PERSON_NOT_ARRIVED) {
        unlockSharedState(sync);
        return 0;
    }

    person->state = PERSON_WAITING;

    int startFloor = person->startFloor;
    int targetFloor = person->targetFloor;
    int personId = person->id;
    int currentTime = state->currentTime;

    Direction direction = targetFloor > startFloor
        ? DIRECTION_UP
        : DIRECTION_DOWN;

    logEvent(
        "[Time %d] Person %d appeared on floor %d and wants to go to floor %d.\n",
        currentTime,
        personId,
        startFloor,
        targetFloor
    );
    logEvent(
        "[Time %d] Call %d was registered: floor %d, direction %s.\n",
        currentTime,
        personId,
        startFloor,
        direction == DIRECTION_UP ? "up" : "down"
    );

    if (unlockSharedState(sync) == -1) {
        return -1;
    }

    Call call = {
        .personIndex = personIndex,
        .startFloor = startFloor,
        .direction = direction
    };

    if (enqueueSharedCall(state, sync, &call) == -1) {
        if (lockSharedState(sync) == 0) {
            person = &state->people[personIndex];
            if (person->state == PERSON_WAITING &&
                person->assignedLift == -1) {
                person->state = PERSON_NOT_ARRIVED;
            }
            unlockSharedState(sync);
        }

        return -1;
    }

    return 0;
}
