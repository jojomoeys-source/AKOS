#ifndef INPUT_H
#define INPUT_H

#include <stdio.h>

#include "config.h"
#include "lift.h"
#include "person.h"

typedef struct {
    SimulationConfig config; // Общие параметры модели
    Lift *lifts;             // Массив лифтов
    Person *people;          // Массив пассажиров
} SimulationData;

int readSimulationData(
    SimulationData *data,
    FILE *inputStream,
    int showPrompts
);
void freeSimulationData(SimulationData *data);

#endif
