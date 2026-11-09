#define _POSIX_C_SOURCE 200809L

#include "dispatcher_process.h"

#include "dispatcher.h"
#include "logger.h"
#include "shared_queue.h"

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

        perror("nanosleep dispatcher retry");
        return -1;
    }

    return 0;
}

static int readSimulationStatus(
    SharedState *state,
    IpcSync *sync,
    SimulationStatus *status
) {
    if (lockSharedState(sync) == -1) {
        return -1;
    }

    *status = state->status;

    return unlockSharedState(sync);
}

static void assignPersonToLift(
    Lift *lift,
    Person *person,
    int doorOpeningTime,
    int currentTime
) {
    // Резервирование места и назначение пассажира выбранному лифту
    person->assignedLift = lift->id;
    lift->reservedPlaces++;

    logEvent(
        "[Time %d] Person %d was assigned to lift %d.\n",
        currentTime,
        person->id,
        lift->id
    );

    if (lift->state != LIFT_IDLE) {
        return;
    }

    lift->targetFloor = person->startFloor;

    if (lift->targetFloor > lift->currentFloor) {
        lift->direction = DIRECTION_UP;
        lift->state = LIFT_MOVING;
    } else if (lift->targetFloor < lift->currentFloor) {
        lift->direction = DIRECTION_DOWN;
        lift->state = LIFT_MOVING;
    } else {
        lift->direction = DIRECTION_IDLE;
        lift->state = LIFT_DOORS_OPENING;
        lift->actionTimeLeft = doorOpeningTime;
        lift->stopCount++;
        logEvent(
            "[Time %d] Lift %d started opening its doors at floor %d.\n",
            currentTime,
            lift->id,
            lift->currentFloor
        );
    }
}

int runDispatcherProcess(SharedState *state, IpcSync *sync) {
    if (state == NULL || sync == NULL ||
        sync->stateLock == SEM_FAILED ||
        sync->callsAvailable == SEM_FAILED ||
        state->initialized != 1) {
        return -1;
    }

    for (;;) {
        SimulationStatus status;
        if (readSimulationStatus(state, sync, &status) == -1) {
            return -1;
        }
        if (status != SIMULATION_RUNNING) {
            return 0;
        }

        Call call;
        if (dequeueSharedCall(state, sync, &call) == -1) {
            if (readSimulationStatus(state, sync, &status) == -1) {
                return -1;
            }

            // При остановке родитель разбудит диспетчер пустым sem_post
            if (status != SIMULATION_RUNNING) {
                return 0;
            }

            return -1;
        }

        if (lockSharedState(sync) == -1) {
            return -1;
        }

        if (state->status != SIMULATION_RUNNING) {
            if (unlockSharedState(sync) == -1) {
                return -1;
            }
            return 0;
        }

        if (call.personIndex < 0 ||
            call.personIndex >= state->config.peopleCount) {
            if (unlockSharedState(sync) == -1) {
                return -1;
            }
            continue;
        }

        Person *person = &state->people[call.personIndex];

        // Вызов мог устареть, если пассажира уже назначили или доставили
        if (person->state != PERSON_WAITING || person->assignedLift != -1) {
            if (unlockSharedState(sync) == -1) {
                return -1;
            }
            continue;
        }

        Lift *lift = chooseLiftForPerson(
            state->lifts,
            state->config.liftCount,
            person,
            state->config.strategy
        );

        if (lift != NULL) {
            assignPersonToLift(
                lift,
                person,
                state->config.doorOpeningTime,
                state->currentTime
            );

            if (unlockSharedState(sync) == -1) {
                return -1;
            }
            continue;
        }

        if (unlockSharedState(sync) == -1) {
            return -1;
        }

        if (waitOneSimulationTick() == -1) {
            return -1;
        }

        if (readSimulationStatus(state, sync, &status) == -1) {
            return -1;
        }
        if (status != SIMULATION_RUNNING) {
            return 0;
        }

        if (enqueueSharedCall(state, sync, &call) == -1) {
            return -1;
        }
    }
}
