#define _POSIX_C_SOURCE 200809L

#include "lift_process.h"

#include "actions.h"
#include "service.h"

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

        perror("nanosleep lift tick");
        return -1;
    }

    return 0;
}

static void processLiftTick(
    SharedState *state,
    Lift *lift,
    int currentTime
) {
    // Выполнение действий лифта за один модельный такт
    if (currentTime > 0) {
        advanceTimedLiftActions(
            lift,
            1,
            state->people,
            state->config.peopleCount,
            currentTime
        );

        for (int step = 0; step < state->config.movementSpeed; step++) {
            moveLifts(lift, 1, currentTime);

            checkStopsAtCurrentFloor(
                lift,
                1,
                state->people,
                state->config.peopleCount,
                currentTime,
                state->config.doorOpeningTime
            );
        }
    }

    serviceLiftsAtCurrentFloor(
        lift,
        1,
        state->people,
        state->config.peopleCount,
        currentTime,
        &state->config
    );
}

int runLiftProcess(SharedState *state, IpcSync *sync, int liftIndex) {
    // Работа процесса одного лифта до общей остановки
    if (state == NULL || sync == NULL ||
        sync->stateLock == SEM_FAILED ||
        state->initialized != 1 ||
        liftIndex < 0 ||
        liftIndex >= state->config.liftCount) {
        return -1;
    }

    int nextTime = 0;

    for (;;) {
        if (lockSharedState(sync) == -1) {
            return -1;
        }

        if (state->status != SIMULATION_RUNNING) {
            if (unlockSharedState(sync) == -1) {
                return -1;
            }
            return 0;
        }

        int currentTime = state->currentTime;

        if (nextTime > currentTime) {
            if (unlockSharedState(sync) == -1) {
                return -1;
            }

            if (waitOneSimulationTick() == -1) {
                return -1;
            }
            continue;
        }

        Lift *lift = &state->lifts[liftIndex];

        while (nextTime <= currentTime &&
               state->status == SIMULATION_RUNNING) {
            processLiftTick(state, lift, nextTime);
            state->completedLiftCount++;
            nextTime++;
        }

        if (unlockSharedState(sync) == -1) {
            return -1;
        }
    }
}
