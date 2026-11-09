#ifndef SHARED_QUEUE_H
#define SHARED_QUEUE_H

#include "shared_state.h"
#include "ipc_sync.h"

int enqueueSharedCall(SharedState *state, IpcSync *sync, const Call *call);
int dequeueSharedCall(SharedState *state, IpcSync *sync, Call *call);

#endif // SHARED_QUEUE_H