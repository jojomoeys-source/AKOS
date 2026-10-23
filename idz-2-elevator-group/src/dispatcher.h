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

void assignLiftsToWaitingPeople(
    Lift *lifts,
    int liftCount,
    Person *people,
    int peopleCount,
    int currentTime,
    DispatchStrategy strategy,
    int doorOpeningTime
);

#endif
