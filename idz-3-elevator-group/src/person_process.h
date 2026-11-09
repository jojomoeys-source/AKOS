#ifndef PERSON_PROCESS_H
#define PERSON_PROCESS_H

#include "ipc_sync.h"
#include "shared_state.h"

int runPersonProcess(SharedState *state, IpcSync *sync, int personIndex);

#endif // PERSON_PROCESS_H
