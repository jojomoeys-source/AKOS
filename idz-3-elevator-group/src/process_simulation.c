#define _POSIX_C_SOURCE 200809L

#include "process_simulation.h"

#include "dispatcher_process.h"
#include "interruption.h"
#include "lift_process.h"
#include "logger.h"
#include "person.h"
#include "person_process.h"
#include "statistics.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef int (*ProcessWorker)(SharedState *state, IpcSync *sync, int index);

typedef struct {
    pid_t pid;
    int mayFinishWhileRunning;
} ChildProcess;

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

static int waitForNanoseconds(long nanoseconds) {
    struct timespec remaining = {
        .tv_sec = 0,
        .tv_nsec = nanoseconds
    };

    while (nanosleep(&remaining, &remaining) == -1) {
        if (errno == EINTR) {
            continue;
        }

        perror("nanosleep process simulation");
        return -1;
    }

    return 0;
}

static int dispatcherWorker(SharedState *state, IpcSync *sync, int index) {
    (void)index;
    return runDispatcherProcess(state, sync);
}

static int startWorker(
    ChildProcess *children,
    int *childCount,
    SharedState *state,
    IpcSync *sync,
    ProcessWorker worker,
    int index,
    int mayFinishWhileRunning
) {
    // Запуск дочернего процесса участника модели
    pid_t childPid = fork();

    if (childPid == -1) {
        perror("fork");
        return -1;
    }

    if (childPid == 0) {
        int result = worker(state, sync, index);
        closeIpcSync(sync);
        _exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    children[*childCount].pid = childPid;
    children[*childCount].mayFinishWhileRunning = mayFinishWhileRunning;
    (*childCount)++;

    return 0;
}

static int checkFinishedChildren(ChildProcess *children, int childCount) {
    for (int i = 0; i < childCount; i++) {
        if (children[i].pid <= 0) {
            continue;
        }

        int childStatus = 0;
        pid_t result;

        do {
            result = waitpid(children[i].pid, &childStatus, WNOHANG);
        } while (result == -1 && errno == EINTR);

        if (result == 0) {
            continue;
        }

        if (result == -1) {
            perror("waitpid WNOHANG");
            children[i].pid = -1;
            return -1;
        }

        children[i].pid = -1;

        int exitedSuccessfully = WIFEXITED(childStatus) &&
            WEXITSTATUS(childStatus) == EXIT_SUCCESS;

        if (!exitedSuccessfully || !children[i].mayFinishWhileRunning) {
            return -1;
        }
    }

    return 0;
}

static int stopAndWaitForChildren(
    SharedState *state,
    IpcSync *sync,
    ChildProcess *children,
    int childCount,
    SimulationStopReason reason
) {
    // Остановка участников и ожидание завершения дочерних процессов
    if (lockSharedState(sync) == -1) {
        return -1;
    }

    if (state->status == SIMULATION_RUNNING) {
        state->status = SIMULATION_STOPPING;
        state->stopReason = reason;
    }

    if (unlockSharedState(sync) == -1) {
        return -1;
    }

    if (sem_post(sync->callsAvailable) == -1) {
        perror("sem_post callsAvailable during shutdown");
    }

    int failedChild = 0;

    for (int i = 0; i < childCount; i++) {
        if (children[i].pid <= 0) {
            continue;
        }

        int childStatus = 0;
        pid_t result;

        do {
            result = waitpid(children[i].pid, &childStatus, 0);
        } while (result == -1 && errno == EINTR);

        if (result == -1) {
            perror("waitpid");
            failedChild = 1;
            continue;
        }

        children[i].pid = -1;

        if (!WIFEXITED(childStatus) ||
            WEXITSTATUS(childStatus) != EXIT_SUCCESS) {
            failedChild = 1;
        }
    }

    if (lockSharedState(sync) == -1) {
        return -1;
    }

    state->status = SIMULATION_FINISHED;
    if (failedChild) {
        state->stopReason = STOP_REASON_ERROR;
    }

    if (unlockSharedState(sync) == -1) {
        return -1;
    }

    return failedChild ? -1 : 0;
}

int runProcessSimulation(SharedState *state, IpcSync *sync) {
    // Родитель запускает диспетчера, пассажиров и лифты
    if (state == NULL || sync == NULL ||
        sync->stateLock == SEM_FAILED ||
        sync->callsAvailable == SEM_FAILED ||
        state->initialized != 1 ||
        state->config.peopleCount < 0 ||
        state->config.peopleCount > MAX_PASSENGERS ||
        state->config.liftCount <= 0 ||
        state->config.liftCount > MAX_LIFTS) {
        return -1;
    }

    ChildProcess children[MAX_PASSENGERS + MAX_LIFTS + 1] = {0};
    int childCount = 0;
    SimulationStopReason stopReason = STOP_REASON_NONE;

    if (startWorker(
            children,
            &childCount,
            state,
            sync,
            dispatcherWorker,
            0,
            0) == -1) {
        stopReason = STOP_REASON_ERROR;
    }

    for (int i = 0;
         i < state->config.liftCount && stopReason == STOP_REASON_NONE;
         i++) {
        if (startWorker(
                children,
                &childCount,
                state,
                sync,
                runLiftProcess,
                i,
                0) == -1) {
            stopReason = STOP_REASON_ERROR;
        }
    }

    for (int i = 0;
         i < state->config.peopleCount && stopReason == STOP_REASON_NONE;
         i++) {
        if (startWorker(
                children,
                &childCount,
                state,
                sync,
                runPersonProcess,
                i,
                1) == -1) {
            stopReason = STOP_REASON_ERROR;
        }
    }

    while (stopReason == STOP_REASON_NONE) {
        if (isInterruptRequested()) {
            stopReason = STOP_REASON_SIGINT;
            break;
        }

        if (checkFinishedChildren(children, childCount) == -1) {
            stopReason = STOP_REASON_ERROR;
            break;
        }

        if (lockSharedState(sync) == -1) {
            stopReason = STOP_REASON_ERROR;
            break;
        }

        int allDelivered = allPeopleDelivered(
            state->people,
            state->config.peopleCount
        );

        if (allDelivered &&
            (state->completedLiftCount >= state->config.liftCount ||
             state->config.peopleCount == 0)) {
            stopReason = STOP_REASON_ALL_DELIVERED;
        } else if (state->completedLiftCount >= state->config.liftCount) {
            if (state->config.workPeriod > 0 &&
                state->currentTime + 1 >= state->config.workPeriod) {
                stopReason = STOP_REASON_WORK_PERIOD;
            } else {
                state->completedLiftCount = 0;
                state->currentTime++;
            }
        }

        int completedLiftCount = state->completedLiftCount;

        if (unlockSharedState(sync) == -1) {
            stopReason = STOP_REASON_ERROR;
            break;
        }

        if (stopReason != STOP_REASON_NONE) {
            break;
        }

        if (completedLiftCount == 0) {
            if (waitForNanoseconds(10 * 1000 * 1000) == -1) {
                stopReason = STOP_REASON_ERROR;
            }
        } else {
            if (waitForNanoseconds(1000 * 1000) == -1) {
                stopReason = STOP_REASON_ERROR;
            }
        }

    }

    int stopResult = stopAndWaitForChildren(
        state,
        sync,
        children,
        childCount,
        stopReason
    );

    if (stopResult == -1) {
        stopReason = STOP_REASON_ERROR;
    }

    if (lockSharedState(sync) == 0) {
        int finalTime = state->config.workPeriod > 0 &&
                        stopReason == STOP_REASON_WORK_PERIOD
            ? state->config.workPeriod
            : state->currentTime;
        unlockSharedState(sync);

        if (stopReason == STOP_REASON_ALL_DELIVERED) {
            logEvent(
                "[Time %d] Simulation finished. All people delivered.\n",
                finalTime
            );
        } else if (stopReason == STOP_REASON_WORK_PERIOD) {
            logEvent(
                "[Time %d] Configured work period finished.\n",
                finalTime
            );
        } else if (stopReason == STOP_REASON_SIGINT) {
            logEvent(
                "\n[Time %d] Simulation interrupted by user.\n",
                finalTime
            );
        } else {
            logEvent("[Time %d] Simulation stopped after an error.\n", finalTime);
        }

        printSimulationSummary(
            state->people,
            state->config.peopleCount,
            state->lifts,
            state->config.liftCount,
            finalTime
        );
    }

    return stopResult == -1 || stopReason == STOP_REASON_ERROR ? -1 : 0;
}

int runIndependentController(SharedState *state, IpcSync *sync) {
    // Ожидание регистрации всех отдельно запущенных участников
    if (state == NULL || sync == NULL ||
        sync->stateLock == SEM_FAILED ||
        sync->callsAvailable == SEM_FAILED ||
        state->initialized != 1 ||
        state->config.liftCount <= 0 ||
        state->config.liftCount > MAX_LIFTS ||
        state->config.peopleCount < 0 ||
        state->config.peopleCount > MAX_PASSENGERS) {
        return -1;
    }

    SimulationStopReason stopReason = STOP_REASON_NONE;
    int startupWaitTicks = 0;

    for (;;) {
        if (isInterruptRequested()) {
            stopReason = STOP_REASON_SIGINT;
            break;
        }

        if (lockSharedState(sync) == -1) {
            stopReason = STOP_REASON_ERROR;
            break;
        }

        int readyCount = state->readyWorkerCount;
        int expectedCount = state->expectedWorkerCount;
        SimulationStatus status = state->status;

        if (status != SIMULATION_INITIALIZING) {
            stopReason = STOP_REASON_ERROR;
        } else if (readyCount >= expectedCount) {
            state->status = SIMULATION_RUNNING;
        }

        int started = state->status == SIMULATION_RUNNING;

        if (unlockSharedState(sync) == -1) {
            stopReason = STOP_REASON_ERROR;
            break;
        }

        if (stopReason != STOP_REASON_NONE || started) {
            break;
        }

        if (startupWaitTicks++ >= 3000) {
            stopReason = STOP_REASON_ERROR;
            break;
        }

        if (waitForNanoseconds(10 * 1000 * 1000) == -1) {
            stopReason = STOP_REASON_ERROR;
            break;
        }
    }

    while (stopReason == STOP_REASON_NONE) {
        if (isInterruptRequested()) {
            stopReason = STOP_REASON_SIGINT;
            break;
        }

        if (lockSharedState(sync) == -1) {
            stopReason = STOP_REASON_ERROR;
            break;
        }

        int allDelivered = allPeopleDelivered(
            state->people,
            state->config.peopleCount
        );

        if (state->workerErrorCount > 0) {
            stopReason = STOP_REASON_ERROR;
        } else if (allDelivered &&
                   (state->completedLiftCount >= state->config.liftCount ||
                    state->config.peopleCount == 0)) {
            stopReason = STOP_REASON_ALL_DELIVERED;
        } else if (state->completedLiftCount >= state->config.liftCount) {
            if (state->config.workPeriod > 0 &&
                state->currentTime + 1 >= state->config.workPeriod) {
                stopReason = STOP_REASON_WORK_PERIOD;
            } else {
                state->completedLiftCount = 0;
                state->currentTime++;
            }
        }

        int completedLiftCount = state->completedLiftCount;

        if (unlockSharedState(sync) == -1) {
            stopReason = STOP_REASON_ERROR;
            break;
        }

        if (stopReason != STOP_REASON_NONE) {
            break;
        }

        long delay = completedLiftCount == 0
            ? 10 * 1000 * 1000
            : 1000 * 1000;

        if (waitForNanoseconds(delay) == -1) {
            stopReason = STOP_REASON_ERROR;
        }
    }

    if (lockSharedState(sync) == -1) {
        return -1;
    }

    state->status = SIMULATION_STOPPING;
    state->stopReason = stopReason;
    int workersToFinish = state->readyWorkerCount;

    if (unlockSharedState(sync) == -1) {
        return -1;
    }

    if (sem_post(sync->callsAvailable) == -1) {
        perror("sem_post callsAvailable during independent shutdown");
    }

    for (;;) {
        if (lockSharedState(sync) == -1) {
            return -1;
        }

        int finishedCount = state->finishedWorkerCount;
        int workerErrorCount = state->workerErrorCount;

        if (unlockSharedState(sync) == -1) {
            return -1;
        }

        if (finishedCount >= workersToFinish) {
            if (workerErrorCount > 0) {
                stopReason = STOP_REASON_ERROR;
            }
            break;
        }

        if (waitForNanoseconds(10 * 1000 * 1000) == -1) {
            return -1;
        }
    }

    if (lockSharedState(sync) == -1) {
        return -1;
    }

    state->status = SIMULATION_FINISHED;
    state->stopReason = stopReason;
    int finalTime = state->config.workPeriod > 0 &&
                    stopReason == STOP_REASON_WORK_PERIOD
        ? state->config.workPeriod
        : state->currentTime;

    if (unlockSharedState(sync) == -1) {
        return -1;
    }

    if (stopReason == STOP_REASON_ALL_DELIVERED) {
        logEvent(
            "[Time %d] Simulation finished. All people delivered.\n",
            finalTime
        );
    } else if (stopReason == STOP_REASON_WORK_PERIOD) {
        logEvent(
            "[Time %d] Configured work period finished.\n",
            finalTime
        );
    } else if (stopReason == STOP_REASON_SIGINT) {
        logEvent("\n[Time %d] Simulation interrupted by user.\n", finalTime);
    } else {
        logEvent("[Time %d] Simulation stopped after an error.\n", finalTime);
    }

    printSimulationSummary(
        state->people,
        state->config.peopleCount,
        state->lifts,
        state->config.liftCount,
        finalTime
    );

    return stopReason == STOP_REASON_ERROR ? -1 : 0;
}
