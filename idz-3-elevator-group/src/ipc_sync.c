#define _POSIX_C_SOURCE 200809l

#include "ipc_sync.h"

#include <fcntl.h>
#include <stdio.h>

int createIpcSync(IpcSync *sync, const char *stateLockName, const char *callsAvailableName) {
    // Создание именованных POSIX-семафоров
    if (sync == NULL || stateLockName == NULL || callsAvailableName == NULL) {
        return -1;
    }

    sync->stateLock = SEM_FAILED;
    sync->callsAvailable = SEM_FAILED;
    sync->named = 1;

    sync->stateLock = sem_open(stateLockName, O_CREAT | O_EXCL, 0600, 1);
    if (sync->stateLock == SEM_FAILED) {
        perror("sem_open stateLock");
        return -1;
    }

    sync->callsAvailable = sem_open(callsAvailableName, O_CREAT | O_EXCL, 0600, 0);
    if (sync->callsAvailable == SEM_FAILED) {
        perror("sem_open callsAvailable");
        sem_close(sync->stateLock);
        sync->stateLock = SEM_FAILED;
        sem_unlink(stateLockName);
        sync->named = 0;
        return -1;
    }

    return 0;
}

int createUnnamedIpcSync(SharedState *state, IpcSync *sync) {
    // Инициализация семафоров, расположенных в общей памяти
    if (state == NULL || sync == NULL) {
        return -1;
    }

    sync->stateLock = SEM_FAILED;
    sync->callsAvailable = SEM_FAILED;
    sync->named = 0;

    if (sem_init(&state->processStateLock, 1, 1) == -1) {
        perror("sem_init processStateLock");
        return -1;
    }

    if (sem_init(&state->processCallsAvailable, 1, 0) == -1) {
        perror("sem_init processCallsAvailable");
        sem_destroy(&state->processStateLock);
        return -1;
    }

    sync->stateLock = &state->processStateLock;
    sync->callsAvailable = &state->processCallsAvailable;
    return 0;
}

int openNamedIpcSync(
    IpcSync *sync,
    const char *stateLockName,
    const char *callsAvailableName
) {
    // Подключение к именованным семафорам независимых процессов
    if (sync == NULL || stateLockName == NULL || callsAvailableName == NULL) {
        return -1;
    }

    sync->stateLock = SEM_FAILED;
    sync->callsAvailable = SEM_FAILED;
    sync->named = 1;

    sync->stateLock = sem_open(stateLockName, 0);
    if (sync->stateLock == SEM_FAILED) {
        perror("sem_open existing stateLock");
        return -1;
    }

    sync->callsAvailable = sem_open(callsAvailableName, 0);
    if (sync->callsAvailable == SEM_FAILED) {
        perror("sem_open existing callsAvailable");
        sem_close(sync->stateLock);
        sync->stateLock = SEM_FAILED;
        sync->named = 0;
        return -1;
    }

    return 0;
}

int destroyUnnamedIpcSync(SharedState *state) {
    // Уничтожение неименованных семафоров перед освобождением общей памяти
    if (state == NULL) {
        return -1;
    }

    int result = 0;

    if (sem_destroy(&state->processCallsAvailable) == -1) {
        perror("sem_destroy processCallsAvailable");
        result = -1;
    }

    if (sem_destroy(&state->processStateLock) == -1) {
        perror("sem_destroy processStateLock");
        result = -1;
    }

    return result;
}

void closeIpcSync(IpcSync *sync) {
    if (sync == NULL) {
        return;
    }

    if (!sync->named) {
        return;
    }

    if (sync->stateLock != SEM_FAILED) {
        if (sem_close(sync->stateLock) == -1) {
            perror("sem_close stateLock");
        }
        sync->stateLock = SEM_FAILED;
    }

    if (sync->callsAvailable != SEM_FAILED) {
        if (sem_close(sync->callsAvailable) == -1) {
            perror("sem_close callsAvailable");
        }
        sync->callsAvailable = SEM_FAILED;
    }

    sync->named = 0;
}

int removeIpcSync(const char *stateLockName, const char *callsAvailableName) {
    // Удаление имён семафоров после завершения процессов
    if (stateLockName == NULL || callsAvailableName == NULL) {
        return -1;
    }

    int result = 0;

    if (sem_unlink(stateLockName) == -1) {
        perror("sem_unlink_stateLock");
        result = -1;
    }

    if (sem_unlink(callsAvailableName) == -1) {
        perror("sem_unlink_callsAvailable");
        result = -1;
    }

    return result;
}
