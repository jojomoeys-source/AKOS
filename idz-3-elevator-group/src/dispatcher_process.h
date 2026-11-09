#ifndef DISPATCHER_PROCESS_H
#define DISPATCHER_PROCESS_H

#include "ipc_sync.h"
#include "shared_state.h"

int runDispatcherProcess(SharedState *state, IpcSync *sync);

#endif // DISPATCHER_PROCESS_H