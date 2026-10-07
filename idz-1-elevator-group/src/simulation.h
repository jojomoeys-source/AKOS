#ifndef SIMULATION_H
#define SIMULATION_H

#include "config.h"
#include "lift.h"
#include "person.h"

void processArrivals(Person *personArray, int totalPeople, int currentTime);

void runSimulation(
    Person *personArray,
    int totalPeople,
    Lift *liftArray,
    int liftCount,
    const SimulationConfig *config
);

#endif
