#include "logger.h"
#include "person_thread.h"

// Ожидание появления и формерование вызова
void *personThreadMain(void *argument) {
    PersonThreadArguments *arguments = argument;
    SimulationContext *context = arguments->context;
    int personIndex = arguments->personIndex;

    Person *person = &context->people[personIndex];

    if (!waitForSimulationTime(context, person->arrivalTime)) {
        return NULL;
    }

    pthread_mutex_lock(&context->modelMutex);

    if (context->stopRequested ||
        person->state != PERSON_NOT_ARRIVED) {
        pthread_mutex_unlock(&context->modelMutex);
        return NULL;
    }

    person->state = PERSON_WAITING;

    int currentTime = context->currentTime;
    int personId = person->id;
    int startFloor = person->startFloor;
    int targetFloor = person->targetFloor;

    pthread_mutex_unlock(&context->modelMutex);

    Direction direction = targetFloor > startFloor
        ? DIRECTION_UP
        : DIRECTION_DOWN;

    logEvent(
        "[Time %d] Person %d appeared on floor %d "
        "and wants to go to floor %d.\n",
        currentTime,
        personId,
        startFloor,
        targetFloor
    );

    logEvent(
        "[Time %d] Call %d was registered: floor %d, "
        "direction %s.\n",
        currentTime,
        personId,
        startFloor,
        direction == DIRECTION_UP ? "up" : "down"
    );

    Call call = {
        .personIndex = personIndex,
        .startFloor = startFloor,
        .direction = direction
    };

    if (!enqueueCall(&context->callQueue, &call)) {
        logEvent(
            "[Time %d] Error: call of person %d "
            "could not be registered.\n",
            currentTime,
            personId
        );
    }

    return NULL;
}
