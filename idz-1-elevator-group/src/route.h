#ifndef ROUTE_H
#define ROUTE_H

#include "lift.h"
#include "person.h"

int chooseNextStop(
    const Lift *lift,
    const Person *people,
    int peopleCount
);

#endif

