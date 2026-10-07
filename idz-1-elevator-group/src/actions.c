#include "actions.h"
#include "logger.h"

// Завершение действий, на которые ушло несколько тактов
void advanceTimedLiftActions(
    Lift *lifts,
    int liftCount,
    Person *people,
    int peopleCount,
    int currentTime
) {
    (void)peopleCount;

    for (int i = 0; i < liftCount; i++) {
        Lift *lift = &lifts[i];

        if (lift->state != LIFT_DOORS_OPENING &&
            lift->state != LIFT_UNLOADING &&
            lift->state != LIFT_BOARDING) {
            continue;
        }

        lift->actionTimeLeft--;
        if (lift->actionTimeLeft > 0) {
            continue;
        }

        // После открытия дверей можно начинать обслуживание этажа
        if (lift->state == LIFT_DOORS_OPENING) {
            lift->state = LIFT_DOORS_OPEN;
            logEvent("[Time %d] Lift %d opened its doors at floor %d.\n",
                     currentTime, lift->id, lift->currentFloor);
        // После высадки пассажир считается доставленным
        } else if (lift->state == LIFT_UNLOADING) {
            Person *person = &people[lift->activePersonIndex];
            person->state = PERSON_DELIVERED;
            lift->peopleInside--;
            lift->activePersonIndex = -1;
            lift->state = LIFT_DOORS_OPEN;
            logEvent("[Time %d] Person %d left lift %d at floor %d.\n",
                     currentTime, person->id, lift->id, lift->currentFloor);
            logEvent("[Time %d] Person %d was delivered to floor %d.\n",
                     currentTime, person->id, person->targetFloor);
        // Оставшийся вариант состояния означает завершение посадки
        } else {
            Person *person = &people[lift->activePersonIndex];
            person->state = PERSON_IN_LIFT;
            lift->peopleInside++;
            if (lift->reservedPlaces > 0) {
                lift->reservedPlaces--;
            }
            lift->activePersonIndex = -1;
            lift->state = LIFT_DOORS_OPEN;
            logEvent("[Time %d] Person %d entered lift %d.\n",
                     currentTime, person->id, lift->id);
        }
    }
}
