#include "logger.h"
#include "route.h"
#include "service.h"

// Поиск пассажира, которому пора выходить
static int findPersonToUnload(
    const Lift *lift,
    const Person *people,
    int peopleCount
) {
    for (int i = 0; i < peopleCount; i++) {
        if (people[i].state == PERSON_IN_LIFT &&
            people[i].assignedLift == lift->id &&
            people[i].targetFloor == lift->currentFloor) {
            return i;
        }
    }
    return -1;
}

// Поиск пассажира для посадки в лифт
static int findPersonToBoard(
    const Lift *lift,
    const Person *people,
    int peopleCount
) {
    if (lift->peopleInside >= lift->capacity) {
        return -1;
    }

    for (int i = 0; i < peopleCount; i++) {
        if (people[i].state == PERSON_WAITING &&
            people[i].assignedLift == lift->id &&
            people[i].startFloor == lift->currentFloor) {
            return i;
        }
    }
    return -1;
}

// Проверка ожидающих пассажиров на текущем этаже
static int hasAssignedPersonAtFloor(
    const Lift *lift,
    const Person *people,
    int peopleCount
) {
    return findPersonToBoard(lift, people, peopleCount) != -1;
}

// Снятие назначения с пассажиров, которым не хватило места
static void releasePeopleWhoDidNotFit(
    Lift *lift,
    Person *people,
    int peopleCount,
    int currentTime
) {
    for (int i = 0; i < peopleCount; i++) {
        Person *person = &people[i];

        if (person->state == PERSON_WAITING &&
            person->assignedLift == lift->id &&
            person->startFloor == lift->currentFloor) {
            person->assignedLift = -1;
            if (lift->reservedPlaces > 0) {
                lift->reservedPlaces--;
            }
            logEvent("[Time %d] Person %d could not enter lift %d "
                     "because it is full; a new call is required.\n",
                     currentTime, person->id, lift->id);
        }
    }
}

// Закрытие дверей и выбор следующей остановки
static void closeDoorsAndSetNextTarget(
    Lift *lift,
    Person *people,
    int peopleCount,
    int currentTime
) {
    int nextStop = chooseNextStop(lift, people, peopleCount);

    logEvent("[Time %d] Lift %d closed its doors at floor %d.\n",
             currentTime, lift->id, lift->currentFloor);

    if (nextStop == -1) {
        lift->state = LIFT_IDLE;
        lift->direction = DIRECTION_IDLE;
        lift->targetFloor = lift->currentFloor;
        logEvent("[Time %d] Lift %d is now idle.\n",
                 currentTime, lift->id);
        return;
    }

    lift->targetFloor = nextStop;
    lift->direction = nextStop > lift->currentFloor
        ? DIRECTION_UP
        : DIRECTION_DOWN;
    lift->state = LIFT_MOVING;

    logEvent("[Time %d] Lift %d started moving %s to floor %d.\n",
             currentTime,
             lift->id,
             lift->direction == DIRECTION_UP ? "up" : "down",
             lift->targetFloor);
}

// Обслуживание лифта на текущем этаже
void serviceLiftsAtCurrentFloor(
    Lift *lifts,
    int liftCount,
    Person *people,
    int peopleCount,
    int currentTime,
    const SimulationConfig *config
) {
    for (int i = 0; i < liftCount; i++) {
        Lift *lift = &lifts[i];

        if (lift->state == LIFT_IDLE &&
            hasAssignedPersonAtFloor(lift, people, peopleCount)) {
            lift->state = LIFT_DOORS_OPENING;
            lift->actionTimeLeft = config->doorOpeningTime;
            lift->stopCount++;
            logEvent("[Time %d] Lift %d started opening its doors "
                     "at floor %d.\n",
                     currentTime, lift->id, lift->currentFloor);
        }

        if (lift->state != LIFT_DOORS_OPEN) {
            continue;
        }

        // Сначала выпускаем пассажиров, затем начинаем посадку
        int personIndex = findPersonToUnload(lift, people, peopleCount);
        if (personIndex != -1) {
            lift->state = LIFT_UNLOADING;
            lift->activePersonIndex = personIndex;
            lift->actionTimeLeft = config->unloadingTime;
            logEvent("[Time %d] Person %d started leaving lift %d.\n",
                     currentTime, people[personIndex].id, lift->id);
            continue;
        }

        // Посадка начинается только при наличии свободного места
        personIndex = findPersonToBoard(lift, people, peopleCount);
        if (personIndex != -1) {
            lift->state = LIFT_BOARDING;
            lift->activePersonIndex = personIndex;
            lift->actionTimeLeft = config->boardingTime;
            logEvent("[Time %d] Person %d started entering lift %d.\n",
                     currentTime, people[personIndex].id, lift->id);
            continue;
        }

        // Если обслуживать больше некого, закрываем двери
        releasePeopleWhoDidNotFit(lift, people, peopleCount, currentTime);
        closeDoorsAndSetNextTarget(lift, people, peopleCount, currentTime);
    }
}

// Проверка необходимости остановки на текущем этаже
void checkStopsAtCurrentFloor(
    Lift *lifts,
    int liftCount,
    Person *people,
    int peopleCount,
    int currentTime,
    int doorOpeningTime
) {
    for (int i = 0; i < liftCount; i++) {
        Lift *lift = &lifts[i];
        int shouldStop = lift->currentFloor == lift->targetFloor;

        if (lift->state != LIFT_MOVING) {
            continue;
        }

        for (int j = 0; j < peopleCount && !shouldStop; j++) {
            Person *person = &people[j];
            shouldStop =
                (person->state == PERSON_IN_LIFT &&
                 person->assignedLift == lift->id &&
                 person->targetFloor == lift->currentFloor) ||
                (person->state == PERSON_WAITING &&
                 person->assignedLift == lift->id &&
                 person->startFloor == lift->currentFloor &&
                 lift->peopleInside < lift->capacity);
        }

        if (shouldStop) {
            lift->state = LIFT_DOORS_OPENING;
            lift->actionTimeLeft = doorOpeningTime;
            lift->stopCount++;
            logEvent("[Time %d] Lift %d arrived at floor %d and started "
                     "opening its doors.\n",
                     currentTime, lift->id, lift->currentFloor);
        }
    }
}
