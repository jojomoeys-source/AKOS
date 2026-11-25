#ifndef LIFT_PROCESS_H
#define LIFT_PROCESS_H

#include "ipc_sync.h"
#include "shared_state.h"

int runLiftProcess(SharedState *state, IpcSync *sync, int liftIndex);

#endif // LIFT_PROCESS_H
