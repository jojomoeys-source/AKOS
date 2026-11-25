#ifndef PROCESS_SIMULATION_H
#define PROCESS_SIMULATION_H

#include "shared_state.h"
#include "ipc_sync.h"

int runProcessSimulation(SharedState *state, IpcSync *sync);
int runIndependentController(SharedState *state, IpcSync *sync);

#endif // PROCESS_SIMULATION_H
