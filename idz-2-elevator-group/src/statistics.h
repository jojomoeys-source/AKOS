#ifndef STATISTICS_H
#define STATISTICS_H

#include "lift.h"
#include "person.h"

void printSimulationSummary(
    const Person *personArray,
    int totalPeople,
    const Lift *liftArray,
    int liftCount,
    int currentTime
);

#endif

