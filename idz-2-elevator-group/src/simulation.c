#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "actions.h"
#include "context.h"
#include "dispatcher.h"
#include "dispatcher_thread.h"
#include "interruption.h"
#include "logger.h"
#include "person_thread.h"
#include "service.h"
#include "simulation.h"
#include "statistics.h"

// Потокобезопасная проверка доставки всех пассажиров
static int contextAllPeopleDelivered(SimulationContext *context) {
    pthread_mutex_lock(&context->modelMutex);
    int delivered = allPeopleDelivered(
        context->people,
        context->peopleCount
    );
    pthread_mutex_unlock(&context->modelMutex);

    return delivered;
}

// Остановка и ожидание всех созданных потоков
static void stopWorkerThreads(
    SimulationContext *context,
    pthread_t dispatcherThread,
    pthread_t *personThreads,
    int personThreadCount
) {
    requestSimulationStop(context);

    for (int i = 0; i < personThreadCount; i++) {
        pthread_join(personThreads[i], NULL);
    }

    pthread_join(dispatcherThread, NULL);
}

// Движение лифтов с учётом заданной скорости
static void moveLiftsForOneTick(
    Lift *lifts,
    int liftCount,
    Person *people,
    int peopleCount,
    int currentTime,
    const SimulationConfig *config
) {
    for (int step = 0; step < config->movementSpeed; step++) {
        moveLifts(lifts, liftCount, currentTime);
        checkStopsAtCurrentFloor(
            lifts,
            liftCount,
            people,
            peopleCount,
            currentTime,
            config->doorOpeningTime
        );
    }
}

// Главный цикл симуляции
void runSimulation(
    Person *people,
    int peopleCount,
    Lift *lifts,
    int liftCount,
    const SimulationConfig *config
) {
    SimulationContext context;

    if (!initializeSimulationContext(
        &context,
        people,
        peopleCount,
        lifts,
        liftCount,
        config)) {
        logEvent("Error: could not initialize simulation context.\n");
        return;
    }

    pthread_t dispatcherThread;

    int errorCode = pthread_create(
        &dispatcherThread,
        NULL,
        dispatcherThreadMain,
        &context
    );

    if (errorCode != 0) {
        logEvent(
            "Error: could not create dispatcher thread: %s\n",
            strerror(errorCode)
        );
        destroySimulationContext(&context);
        return;
    }

    pthread_t *personThreads = calloc(
        (size_t)peopleCount,
        sizeof(pthread_t)
    );
    PersonThreadArguments *personArguments = calloc(
        (size_t)peopleCount,
        sizeof(PersonThreadArguments)
    );

    if (personThreads == NULL || personArguments == NULL) {
        logEvent(
            "Error: could not allocate passenger thread data.\n"
        );

        free(personThreads);
        free(personArguments);

        requestSimulationStop(&context);
        pthread_join(dispatcherThread, NULL);
        destroySimulationContext(&context);
        return;
    }

    int createdPersonThreads = 0;

    for (int i = 0; i < peopleCount; i++) {
        personArguments[i].context = &context;
        personArguments[i].personIndex = i;

        errorCode = pthread_create(
            &personThreads[i],
            NULL,
            personThreadMain,
            &personArguments[i]
        );
        if (errorCode != 0) {
            logEvent(
                "Error: could not create thread for person %d: %s\n",
                people[i].id,
                strerror(errorCode)
            );
            break;
        }

        createdPersonThreads++;
    }

    if (createdPersonThreads != peopleCount) {
        stopWorkerThreads(
            &context,
            dispatcherThread,
            personThreads,
            createdPersonThreads
        );
        free(personArguments);
        free(personThreads);
        destroySimulationContext(&context);
        return;
    }

    int currentTime = 0;
    while (!isInterruptRequested() &&
           !contextAllPeopleDelivered(&context) &&
           (config->workPeriod == 0 || currentTime < config->workPeriod)) {
        pthread_mutex_lock(&context.modelMutex);

        // Сначала заканчиваем действия из прошлых тактов
        if (currentTime > 0) {
            advanceTimedLiftActions(
                lifts, liftCount, people, peopleCount, currentTime
            );
            moveLiftsForOneTick(
                lifts,
                liftCount,
                people,
                peopleCount,
                currentTime,
                config
            );
        }

        // Теперь можно обрабатывать новые события
        assignLiftsToWaitingPeople(
            lifts,
            liftCount,
            people,
            peopleCount,
            currentTime,
            config->strategy,
            config->doorOpeningTime
        );
        serviceLiftsAtCurrentFloor(
            lifts,
            liftCount,
            people,
            peopleCount,
            currentTime,
            config
        );

        int delivered = allPeopleDelivered(people, peopleCount);
        pthread_mutex_unlock(&context.modelMutex);

        if (delivered) {
            stopWorkerThreads(
                &context,
                dispatcherThread,
                personThreads,
                createdPersonThreads
            );

            logEvent(
                "[Time %d] Simulation finished. All people delivered.\n",
                currentTime
            );
            printSimulationSummary(
                people, peopleCount, lifts, liftCount, currentTime
            );

            free(personArguments);
            free(personThreads);
            destroySimulationContext(&context);
            return;
        }

        currentTime++;
        setSimulationTime(&context, currentTime);
    }

    if (isInterruptRequested()) {
        logEvent("\n[Time %d] Simulation interrupted by user.\n",
                 currentTime);
    } else if (config->workPeriod > 0 &&
               currentTime >= config->workPeriod) {
        logEvent("[Time %d] Configured work period finished.\n",
                 currentTime);
    }

    stopWorkerThreads(
        &context,
        dispatcherThread,
        personThreads,
        createdPersonThreads
    );

    printSimulationSummary(
        people, peopleCount, lifts, liftCount, currentTime
    );

    free(personArguments);
    free(personThreads);
    destroySimulationContext(&context);
}
