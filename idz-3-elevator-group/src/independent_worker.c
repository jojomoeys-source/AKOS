#define _POSIX_C_SOURCE 200809L

#include "independent_worker.h"

#include "dispatcher_process.h"
#include "ipc_names.h"
#include "ipc_sync.h"
#include "lift_process.h"
#include "logger.h"
#include "person_process.h"
#include "shared_state.h"

#include <errno.h>
#include <stdio.h>
#include <time.h>

static int lockSharedState(IpcSync *sync) {
    while (sem_wait(sync->stateLock) == -1) {
        if (errno == EINTR) {
            continue;
        }

        perror("sem_wait independent worker");
        return -1;
    }

    return 0;
}

static int unlockSharedState(IpcSync *sync) {
    if (sem_post(sync->stateLock) == -1) {
        perror("sem_post independent worker");
        return -1;
    }

    return 0;
}

static int waitOneTick(void) {
    struct timespec remaining = {
        .tv_sec = 0,
        .tv_nsec = 10 * 1000 * 1000
    };

    while (nanosleep(&remaining, &remaining) == -1) {
        if (errno == EINTR) {
            continue;
        }

        perror("nanosleep independent worker");
        return -1;
    }

    return 0;
}

static int executeWorker(
    IndependentWorkerRole role,
    SharedState *state,
    IpcSync *sync,
    int index
) {
    switch (role) {
        case INDEPENDENT_PERSON:
            return runPersonProcess(state, sync, index);
        case INDEPENDENT_DISPATCHER:
            return runDispatcherProcess(state, sync);
        case INDEPENDENT_LIFT:
            return runLiftProcess(state, sync, index);
    }

    return -1;
}

int runIndependentWorker(
    IndependentWorkerRole role,
    int index,
    const char *logPath
) {
    // Подключение отдельно запущенного участника к общей модели
    if (logPath == NULL || !openLogAppend(logPath)) {
        return -1;
    }

    int sharedMemoryFd = -1;
    size_t sharedMemorySize = 0;
    SharedState *state = openSharedState(
        ELEVATOR_SHARED_MEMORY_NAME,
        &sharedMemoryFd,
        &sharedMemorySize
    );
    if (state == NULL) {
        closeLog();
        return -1;
    }

    IpcSync sync = {
        .stateLock = SEM_FAILED,
        .callsAvailable = SEM_FAILED,
        .named = 0
    };

    if (openNamedIpcSync(
            &sync,
            ELEVATOR_STATE_LOCK_NAME,
            ELEVATOR_CALLS_AVAILABLE_NAME) == -1) {
        closeSharedState(state, sharedMemoryFd, sharedMemorySize);
        closeLog();
        return -1;
    }

    if (lockSharedState(&sync) == -1) {
        closeIpcSync(&sync);
        closeSharedState(state, sharedMemoryFd, sharedMemorySize);
        closeLog();
        return -1;
    }

    int validWorker = state->status == SIMULATION_INITIALIZING &&
        state->readyWorkerCount < state->expectedWorkerCount;

    if (validWorker) {
        // Сообщение управляющему процессу о готовности участника
        state->readyWorkerCount++;
    }

    if (unlockSharedState(&sync) == -1 || !validWorker) {
        closeIpcSync(&sync);
        closeSharedState(state, sharedMemoryFd, sharedMemorySize);
        closeLog();
        return -1;
    }

    int result = 0;
    SimulationStatus status;

    for (;;) {
        if (lockSharedState(&sync) == -1) {
            result = -1;
            break;
        }

        status = state->status;

        if (unlockSharedState(&sync) == -1) {
            result = -1;
            break;
        }

        if (status != SIMULATION_INITIALIZING) {
            break;
        }

        if (waitOneTick() == -1) {
            result = -1;
            break;
        }
    }

    if (result == 0 && status == SIMULATION_RUNNING) {
        result = executeWorker(role, state, &sync, index);
    }

    if (lockSharedState(&sync) == 0) {
        state->finishedWorkerCount++;
        if (result != 0) {
            state->workerErrorCount++;
        }
        unlockSharedState(&sync);
    } else {
        result = -1;
    }

    closeIpcSync(&sync);
    closeSharedState(state, sharedMemoryFd, sharedMemorySize);
    closeLog();
    return result;
}
