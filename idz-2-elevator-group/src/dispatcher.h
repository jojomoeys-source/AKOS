#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "config.h"
#include "lift.h"
#include "person.h"

Direction getPersonDirection(const Person *person);
Lift *chooseLiftForPerson(
    Lift *lifts,
    int liftCount,
    const Person *person,
    DispatchStrategy strategy
);

int assignLiftToPerson(
    Lift *lifts,
    int liftCount,
    Person *person,
    int currentTime,
    DispatchStrategy strategy,
    int doorOpeningTime
);

#endif
