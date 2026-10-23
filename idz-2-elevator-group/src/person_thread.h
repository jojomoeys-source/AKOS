#ifndef PERSON_THREAD_H
#define PERSON_THREAD_H

#include "context.h"

typedef struct {
    SimulationContext *context;
    int personIndex;
} PersonThreadArguments;

// Функция потока пассажира
void *personThreadMain(void *argument);

#endif
