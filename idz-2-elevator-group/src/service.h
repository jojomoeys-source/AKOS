#ifndef SERVICE_H
#define SERVICE_H

#include "config.h"
#include "lift.h"
#include "person.h"

void serviceLiftsAtCurrentFloor(
    Lift *liftArray,
    int liftCount,
    Person *personArray,
    int totalPeople,
    int currentTime,
    const SimulationConfig *config
);

void checkStopsAtCurrentFloor(
    Lift *liftArray,
    int liftCount,
    Person *personArray,
    int totalPeople,
    int currentTime,
    int doorOpeningTime
);

#endif
