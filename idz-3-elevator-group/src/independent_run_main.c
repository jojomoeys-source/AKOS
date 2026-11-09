#define _POSIX_C_SOURCE 200809L

#include "interruption.h"
#include "ipc_names.h"
#include "ipc_sync.h"
#include "logger.h"
#include "process_simulation.h"
#include "shared_state.h"

#include <stdio.h>

int main(void) {
    // Подключение управляющей программы к общей модели
    if (!openLogAppend("elevator.log")) {
        return 1;
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
        return 1;
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
        return 1;
    }

    if (!setupInterruptHandler()) {
        closeIpcSync(&sync);
        closeSharedState(state, sharedMemoryFd, sharedMemorySize);
        closeLog();
        return 1;
    }

    int result = runIndependentController(state, &sync);

    closeIpcSync(&sync);
    closeSharedState(state, sharedMemoryFd, sharedMemorySize);
    closeLog();
    return result == 0 ? 0 : 1;
}
