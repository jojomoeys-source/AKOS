#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include "input.h"
#include "ipc_names.h"
#include "ipc_sync.h"
#include "logger.h"
#include "shared_state.h"

int main(int argumentCount, char **argumentValues) {
    // Чтение конфигурации перед созданием общей памяти
    if (argumentCount != 2) {
        fprintf(stderr, "Usage: %s CONFIG_FILE\n", argumentValues[0]);
        return 1;
    }

    FILE *inputStream = fopen(argumentValues[1], "r");
    if (inputStream == NULL) {
        perror("fopen configuration");
        return 1;
    }

    SimulationData data = {0};
    int inputRead = readSimulationData(&data, inputStream, 0);
    fclose(inputStream);

    if (!inputRead) {
        return 1;
    }

    if (data.config.peopleCount > MAX_PASSENGERS ||
        data.config.liftCount <= 0 ||
        data.config.liftCount > MAX_LIFTS) {
        fprintf(stderr, "Input exceeds shared memory limits.\n");
        freeSimulationData(&data);
        return 1;
    }

    if (!openLog("elevator.log")) {
        freeSimulationData(&data);
        return 1;
    }
    closeLog();

    int sharedMemoryFd = -1;
    size_t sharedMemorySize = 0;
    SharedState *state = createSharedState(
        ELEVATOR_SHARED_MEMORY_NAME,
        &sharedMemoryFd,
        &sharedMemorySize
    );
    if (state == NULL) {
        freeSimulationData(&data);
        return 1;
    }

    IpcSync sync = {
        .stateLock = SEM_FAILED,
        .callsAvailable = SEM_FAILED,
        .named = 0
    };
    if (createIpcSync(
            &sync,
            ELEVATOR_STATE_LOCK_NAME,
            ELEVATOR_CALLS_AVAILABLE_NAME) == -1) {
        closeSharedState(state, sharedMemoryFd, sharedMemorySize);
        removeSharedState(ELEVATOR_SHARED_MEMORY_NAME);
        freeSimulationData(&data);
        return 1;
    }

    state->config = data.config;
    memcpy(
        state->people,
        data.people,
        (size_t)data.config.peopleCount * sizeof(data.people[0])
    );
    memcpy(
        state->lifts,
        data.lifts,
        (size_t)data.config.liftCount * sizeof(data.lifts[0])
    );
    state->expectedWorkerCount =
        data.config.peopleCount + data.config.liftCount + 1;
    state->status = SIMULATION_INITIALIZING;
    state->initialized = 1;

    closeIpcSync(&sync);
    closeSharedState(state, sharedMemoryFd, sharedMemorySize);
    freeSimulationData(&data);

    printf("Independent-process simulation initialized\n");
    printf("Start elevator_dispatcher, one elevator_lift per lift, and one elevator_person per person\n");
    printf("Then run elevator_run; after it finishes, run elevator_cleanup\n");
    return 0;
}
