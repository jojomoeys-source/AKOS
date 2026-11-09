#ifndef SIMULATION_H
#define SIMULATION_H

#include "config.h"
#include "lift.h"
#include "person.h"

void runSimulation(
    Person *personArray,
    int totalPeople,
    Lift *liftArray,
    int liftCount,
    const SimulationConfig *config
);

#endif
