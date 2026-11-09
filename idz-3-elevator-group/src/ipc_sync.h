#ifndef IPC_SYNC_H
#define IPC_SYNC_H

#include <semaphore.h>
#include <stddef.h>

#include "shared_state.h"

typedef struct {
    sem_t *stateLock;
    sem_t *callsAvailable;
    int named;
} IpcSync;

int createIpcSync(IpcSync *sync, const char *stateLockName, const char *callsAvailableName);
int openNamedIpcSync(IpcSync *sync, const char *stateLockName, const char *callsAvailableName);
int createUnnamedIpcSync(SharedState *state, IpcSync *sync);
int destroyUnnamedIpcSync(SharedState *state);
void closeIpcSync(IpcSync *sync);

int removeIpcSync(const char *stateLockName, const char *callsAvailableName);

#endif // IPC_SYNC_H
