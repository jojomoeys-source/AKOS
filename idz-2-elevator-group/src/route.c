#include <stdlib.h>

#include "route.h"

// Поиск следующей остановки по ходу движения лифта
int chooseNextStop(
    const Lift *lift,
    const Person *people,
    int peopleCount
) {
    int directionalStop = -1;
    int nearestStop = -1;
    int nearestDistance = 1000000;

    for (int i = 0; i < peopleCount; i++) {
        const Person *person = &people[i];
        int stopFloor;

        if (person->assignedLift != lift->id) {
            continue;
        }

        if (person->state == PERSON_WAITING) {
            stopFloor = person->startFloor;
        } else if (person->state == PERSON_IN_LIFT) {
            stopFloor = person->targetFloor;
        } else {
            continue;
        }

        if (stopFloor == lift->currentFloor) {
            continue;
        }

        // Сначала запоминаем ближайшую остановку по ходу движения
        if (lift->direction == DIRECTION_UP &&
            stopFloor > lift->currentFloor &&
            (directionalStop == -1 || stopFloor < directionalStop)) {
            directionalStop = stopFloor;
        } else if (lift->direction == DIRECTION_DOWN &&
                   stopFloor < lift->currentFloor &&
                   (directionalStop == -1 || stopFloor > directionalStop)) {
            directionalStop = stopFloor;
        }

        // Ближайшая остановка пригодится после смены направления
        int distance = abs(stopFloor - lift->currentFloor);
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearestStop = stopFloor;
        }
    }

    return directionalStop != -1 ? directionalStop : nearestStop;
}
