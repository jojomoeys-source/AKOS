#include <stdio.h>
#include <string.h>

#include "input.h"
#include "interruption.h"
#include "ipc_sync.h"
#include "ipc_names.h"
#include "logger.h"
#include "options.h"
#include "process_simulation.h"
#include "shared_state.h"

// Точка входа в программу
int main(int argumentCount, char **argumentValues) {
    SimulationData data = {0};
    ProgramOptions options;
    FILE *inputStream = stdin;
    int showPrompts = 1;

    const char *sharedMemoryName = ELEVATOR_SHARED_MEMORY_NAME;
    int sharedMemoryFd = -1;
    size_t sharedMemorySize = 0;
    SharedState *sharedState = NULL;

    const char *stateLockName = ELEVATOR_STATE_LOCK_NAME;
    const char *callsAvailableName = ELEVATOR_CALLS_AVAILABLE_NAME;

    IpcSync ipcSync;
    ipcSync.stateLock = SEM_FAILED;
    ipcSync.callsAvailable = SEM_FAILED;
    ipcSync.named = 0;

    // Сначала разбираем параметры запуска
    OptionsStatus optionsStatus = parseProgramOptions(
        argumentCount,
        argumentValues,
        &options
    );

    if (optionsStatus == OPTIONS_HELP) {
        return 0;
    }
    if (optionsStatus == OPTIONS_ERROR) {
        return 1;
    }

    // При наличии --config читаем данные не из терминала, а из файла
    if (options.configPath != NULL) {
        inputStream = fopen(options.configPath, "r");
        if (inputStream == NULL) {
            perror("Error opening configuration file");
            return 1;
        }
        showPrompts = 0;
    }

    if (!openLog(options.logPath)) {
        if (inputStream != stdin) {
            fclose(inputStream);
        }
        return 1;
    }

    if (!readSimulationData(&data, inputStream, showPrompts)) {
        if (inputStream != stdin) {
            fclose(inputStream);
        }
        closeLog();
        return 1;
    }

    if (inputStream != stdin) {
        fclose(inputStream);
    }

    if (data.config.peopleCount > MAX_PASSENGERS ||
        data.config.liftCount > MAX_LIFTS) {
        fprintf(stderr, "Input exceeds shared memory limits.\n");
        freeSimulationData(&data);
        closeLog();
        return 1;
    }

    sharedState = createSharedState(
        sharedMemoryName,
        &sharedMemoryFd,
        &sharedMemorySize
    );

    if (sharedState == NULL) {
        freeSimulationData(&data);
        closeLog();
        return 1;
    }

    int syncCreated = options.useUnnamedSemaphores
        ? createUnnamedIpcSync(sharedState, &ipcSync)
        : createIpcSync(&ipcSync, stateLockName, callsAvailableName);

    if (syncCreated == -1) {
        freeSimulationData(&data);
        closeSharedState(sharedState, sharedMemoryFd, sharedMemorySize);
        removeSharedState(sharedMemoryName);
        closeLog();
        return 1;
    }

    sharedState->config = data.config;

    memcpy(
        sharedState->people,
        data.people,
        (size_t)data.config.peopleCount * sizeof(data.people[0])
    );

    memcpy(
        sharedState->lifts,
        data.lifts,
        (size_t)data.config.liftCount * sizeof(data.lifts[0])
    );

    sharedState->initialized = 1;
    sharedState->status = SIMULATION_RUNNING;

    // После чтения данных настраиваем корректное завершение по Ctrl+C
    if (!setupInterruptHandler()) {
        freeSimulationData(&data);
        closeIpcSync(&ipcSync);
        if (options.useUnnamedSemaphores) {
            destroyUnnamedIpcSync(sharedState);
        } else {
            removeIpcSync(stateLockName, callsAvailableName);
        }
        closeSharedState(sharedState, sharedMemoryFd, sharedMemorySize);
        removeSharedState(sharedMemoryName);
        closeLog();
        return 1;
    }

    // Запуск процессной версии моделирования
    int simulationResult = runProcessSimulation(sharedState, &ipcSync);
    closeIpcSync(&ipcSync);

    if (options.useUnnamedSemaphores) {
        destroyUnnamedIpcSync(sharedState);
    } else if (removeIpcSync(stateLockName, callsAvailableName) == -1) {
        fprintf(stderr, "Failed to remove IPC semaphores.\n");
    }

    closeSharedState(sharedState, sharedMemoryFd, sharedMemorySize);

    if (removeSharedState(sharedMemoryName) == -1) {
        perror("shm_unlink");
    }

    freeSimulationData(&data);
    closeLog();

    return simulationResult == 0 ? 0 : 1;
}
