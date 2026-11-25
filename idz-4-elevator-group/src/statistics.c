#include "logger.h"
#include "statistics.h"

// Вывод состояния недоставленного пассажира
static void printUndeliveredPersonStatus(const Person *person) {
    logEvent("Person %d: ", person->id);

    switch (person->state) {
        case PERSON_NOT_ARRIVED:
            logEvent("not arrived yet, expected at time %d",
                     person->arrivalTime);
            break;
        case PERSON_WAITING:
            logEvent("waiting at floor %d for floor %d",
                     person->startFloor, person->targetFloor);
            if (person->assignedLift == -1) {
                logEvent(", no lift assigned");
            } else {
                logEvent(", assigned to lift %d", person->assignedLift);
            }
            break;
        case PERSON_IN_LIFT:
            logEvent("inside lift %d, travelling to floor %d",
                     person->assignedLift, person->targetFloor);
            break;
        case PERSON_DELIVERED:
            logEvent("delivered");
            break;
    }

    logEvent(".\n");
}

// Вывод итогов симуляции
void printSimulationSummary(
    const Person *people,
    int peopleCount,
    const Lift *lifts,
    int liftCount,
    int currentTime
) {
    int delivered = 0;

    for (int i = 0; i < peopleCount; i++) {
        if (people[i].state == PERSON_DELIVERED) {
            delivered++;
        }
    }

    logEvent("\nSimulation summary\n");
    logEvent("Time: %d ticks\n", currentTime);
    logEvent("Delivered: %d of %d people\n", delivered, peopleCount);

    for (int i = 0; i < liftCount; i++) {
        logEvent(
            "Lift %d: floor %d, travelled %d floors, made %d stops.\n",
            lifts[i].id,
            lifts[i].currentFloor,
            lifts[i].floorsTravelled,
            lifts[i].stopCount
        );
    }

    if (delivered < peopleCount) {
        logEvent("People not delivered:\n");
        for (int i = 0; i < peopleCount; i++) {
            if (people[i].state != PERSON_DELIVERED) {
                printUndeliveredPersonStatus(&people[i]);
            }
        }
    }
}
