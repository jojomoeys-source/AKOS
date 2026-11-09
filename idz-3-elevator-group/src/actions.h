#ifndef ACTIONS_H
#define ACTIONS_H

#include "lift.h"
#include "person.h"

void advanceTimedLiftActions(
    Lift *liftArray,
    int liftCount,
    Person *personArray,
    int totalPeople,
    int currentTime
);

#endif
