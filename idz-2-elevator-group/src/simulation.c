#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "context.h"
#include "dispatcher_thread.h"
#include "interruption.h"
#include "lift_thread.h"
#include "logger.h"
#include "person_thread.h"
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

// Остановка и ожидание всех созданых потоков
static void stopWorkerThreads(
    SimulationContext *context,
    pthread_t dispatcherThread,
    pthread_t *personThreads,
    int personThreadCount,
    pthread_t *liftThreads,
    int liftThreadCount
) {
    requestSimulationStop(context);

    for (int i = 0; i < personThreadCount; i++) {
        pthread_join(personThreads[i], NULL);
    }

    for (int i = 0; i < liftThreadCount; i++) {
        pthread_join(liftThreads[i], NULL);
    }

    pthread_join(dispatcherThread, NULL);
}

// Небольшая задержка между тактами
static void waitBeforeNextTick(void) {
    struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = 10 * 1000 * 1000
    };

    nanosleep(&delay, NULL);
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
            createdPersonThreads,
            NULL,
            0
        );
        free(personArguments);
        free(personThreads);
        destroySimulationContext(&context);
        return;
    }

    pthread_t *liftThreads = calloc(
        (size_t)liftCount,
        sizeof(pthread_t)
    );
    LiftThreadArguments *liftArguments = calloc(
        (size_t)liftCount,
        sizeof(LiftThreadArguments)
    );

    if (liftThreads == NULL || liftArguments == NULL) {
        logEvent(
            "Error: could not allocate lift thread data.\n"
        );

        free(liftThreads);
        free(liftArguments);

        stopWorkerThreads(
            &context,
            dispatcherThread,
            personThreads,
            createdPersonThreads,
            NULL,
            0
        );

        free(personArguments);
        free(personThreads);
        destroySimulationContext(&context);
        return;
    }

    int createdLiftThreads = 0;

    for (int i = 0; i < liftCount; i++) {
        liftArguments[i].context = &context;
        liftArguments[i].liftIndex = i;

        errorCode = pthread_create(
            &liftThreads[i],
            NULL,
            liftThreadMain,
            &liftArguments[i]
        );
        if (errorCode != 0) {
            logEvent(
                "Error: could not create thread for lift %d: %s\n",
                lifts[i].id,
                strerror(errorCode)
            );
            break;
        }

        createdLiftThreads++;
    }

    if (createdLiftThreads != liftCount) {
        stopWorkerThreads(
            &context,
            dispatcherThread,
            personThreads,
            createdPersonThreads,
            liftThreads,
            createdLiftThreads
        );

        free(liftArguments);
        free(liftThreads);
        free(personArguments);
        free(personThreads);
        destroySimulationContext(&context);
        return;
    }

    int currentTime = 0;

    while (!isInterruptRequested() &&
           !contextAllPeopleDelivered(&context) &&
           (config->workPeriod == 0 ||
            currentTime < config->workPeriod)) {

        //Начинаем новый такт и ждём, пока каждый лифт закончит его обработку.
        setSimulationTime(&context, currentTime);

        if (!waitForAllLifts(&context)) {
            break;
        }

        if (contextAllPeopleDelivered(&context)) {
            stopWorkerThreads(
                &context,
                dispatcherThread,
                personThreads,
                createdPersonThreads,
                liftThreads,
                createdLiftThreads
            );

            logEvent(
                "[Time %d] Simulation finished. "
                "All people delivered.\n",
                currentTime
            );

            printSimulationSummary(
                people,
                peopleCount,
                lifts,
                liftCount,
                currentTime
            );

            free(liftArguments);
            free(liftThreads);
            free(personArguments);
            free(personThreads);
            destroySimulationContext(&context);
            return;
        }

        waitBeforeNextTick();
        currentTime++;
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
        createdPersonThreads,
        liftThreads,
        createdLiftThreads
    );

    printSimulationSummary(
        people, peopleCount, lifts, liftCount, currentTime
    );

    free(liftArguments);
    free(liftThreads);

    free(personArguments);
    free(personThreads);

    destroySimulationContext(&context);
}
