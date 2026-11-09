#include "ipc_names.h"
#include "ipc_sync.h"
#include "shared_state.h"

#include <stdio.h>

int main(void) {
    // Удаление IPC-ресурсов после завершения участников
    int result = 0;

    if (removeIpcSync(
            ELEVATOR_STATE_LOCK_NAME,
            ELEVATOR_CALLS_AVAILABLE_NAME) == -1) {
        perror("remove IPC semaphores");
        result = 1;
    }

    if (removeSharedState(ELEVATOR_SHARED_MEMORY_NAME) == -1) {
        perror("remove shared memory");
        result = 1;
    }

    if (result == 0) {
        puts("Independent-process IPC resources removed");
    }

    return result;
}
