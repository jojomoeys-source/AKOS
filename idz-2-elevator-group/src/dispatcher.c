#include <stddef.h>
#include <stdlib.h>

#include "dispatcher.h"
#include "logger.h"

// Определение направления поездки пассажира
Direction getPersonDirection(const Person *person) {
    return person->targetFloor > person->startFloor
        ? DIRECTION_UP
        : DIRECTION_DOWN;
}

// Проверка, находится ли пассажир по пути движущегося лифта
static int isLiftOnTheWay(const Lift *lift, const Person *person) {
    Direction personDirection = getPersonDirection(person);

    if (lift->state != LIFT_MOVING ||
        lift->direction != personDirection ||
        lift->peopleInside + lift->reservedPlaces >= lift->capacity) {
        return 0;
    }

    if (lift->direction == DIRECTION_UP) {
        return person->startFloor >= lift->currentFloor &&
               person->startFloor <= lift->targetFloor;
    }

    return person->startFloor <= lift->currentFloor &&
           person->startFloor >= lift->targetFloor;
}

// Поиск наиболее подходящего лифта для пассажира
Lift *chooseLiftForPerson(
    Lift *lifts,
    int liftCount,
    const Person *person,
    DispatchStrategy strategy
) {
    Lift *chosenLift = NULL;
    int minDistance = 1000000;

    // Сначала смотрим лифты, которые уже стоят на нужном этаже
    for (int i = 0; i < liftCount; i++) {
        if ((lifts[i].state == LIFT_IDLE ||
             lifts[i].state == LIFT_DOORS_OPEN) &&
            lifts[i].currentFloor == person->startFloor &&
            lifts[i].peopleInside + lifts[i].reservedPlaces <
                lifts[i].capacity) {
            return &lifts[i];
        }
    }

    if (strategy == STRATEGY_ON_THE_WAY) {
        // Затем ищем лифт, который уже движется в нужную сторону
        for (int i = 0; i < liftCount; i++) {
            if (isLiftOnTheWay(&lifts[i], person)) {
                int distance = abs(lifts[i].currentFloor - person->startFloor);

                if (distance < minDistance) {
                    minDistance = distance;
                    chosenLift = &lifts[i];
                }
            }
        }

        if (chosenLift != NULL) {
            return chosenLift;
        }
    }

    minDistance = 1000000;
    // Если попутного нет, берём ближайший свободный лифт
    for (int i = 0; i < liftCount; i++) {
        if (lifts[i].state == LIFT_IDLE &&
            lifts[i].peopleInside + lifts[i].reservedPlaces <
                lifts[i].capacity) {
            int distance = abs(lifts[i].currentFloor - person->startFloor);

            if (distance < minDistance) {
                minDistance = distance;
                chosenLift = &lifts[i];
            }
        }
    }

    return chosenLift;
}

// Назначение лифтов пассажирам, которые пока остались без лифта
void assignLiftsToWaitingPeople(
    Lift *lifts,
    int liftCount,
    Person *people,
    int peopleCount,
    int currentTime,
    DispatchStrategy strategy,
    int doorOpeningTime
) {
    for (int i = 0; i < peopleCount; i++) {
        Person *person = &people[i];

        if (person->state != PERSON_WAITING || person->assignedLift != -1) {
            continue;
        }

        Lift *lift = chooseLiftForPerson(
            lifts, liftCount, person, strategy
        );
        if (lift == NULL) {
            continue;
        }

        person->assignedLift = lift->id;
        lift->reservedPlaces++;

        logEvent("[Time %d] Person %d was assigned to lift %d.\n",
                 currentTime, person->id, lift->id);

        if (lift->state == LIFT_IDLE) {
            lift->targetFloor = person->startFloor;

            if (lift->targetFloor > lift->currentFloor) {
                lift->direction = DIRECTION_UP;
                lift->state = LIFT_MOVING;
            } else if (lift->targetFloor < lift->currentFloor) {
                lift->direction = DIRECTION_DOWN;
                lift->state = LIFT_MOVING;
            } else {
                lift->direction = DIRECTION_IDLE;
                lift->state = LIFT_DOORS_OPENING;
                lift->actionTimeLeft = doorOpeningTime;
                lift->stopCount++;

                logEvent(
                    "[Time %d] Lift %d started opening its doors "
                    "at floor %d.\n",
                    currentTime, lift->id, lift->currentFloor
                );
            }
        }
    }
}
