#include "lift.h"
#include "logger.h"

// Задание начальных значений лифта
void initializeLift(Lift *lift, int id, int capacity, int initialFloor) {
    lift->id = id;
    lift->currentFloor = initialFloor;
    lift->targetFloor = initialFloor;
    lift->capacity = capacity;
    lift->peopleInside = 0;
    lift->reservedPlaces = 0;
    lift->floorsTravelled = 0;
    lift->stopCount = 0;
    lift->actionTimeLeft = 0;
    lift->activePersonIndex = -1;
    lift->direction = DIRECTION_IDLE;
    lift->state = LIFT_IDLE;
}

// Передвижение работающих лифтов на один этаж
void moveLifts(Lift *lifts, int liftCount, int currentTime) {
    for (int i = 0; i < liftCount; i++) {
        Lift *lift = &lifts[i];

        if (lift->state == LIFT_MOVING) {
            if (lift->direction == DIRECTION_UP) {
                lift->currentFloor++;
            } else if (lift->direction == DIRECTION_DOWN) {
                lift->currentFloor--;
            }
            lift->floorsTravelled++;

            logEvent(
                "[Time %d] Lift %d moved %s and arrived at floor %d.\n",
                currentTime,
                lift->id,
                lift->direction == DIRECTION_UP ? "up" : "down",
                lift->currentFloor
            );

        }
    }
}
