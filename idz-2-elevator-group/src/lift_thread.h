#ifndef LIFT_THREAD_H
#define LIFT_THREAD_H

#include "context.h"

typedef struct {
    SimulationContext *context;
    int liftIndex;
} LiftThreadArguments;

// Функция потока лифта
void *liftThreadMain(void *argument);

#endif
