#include <stdio.h>
#include <string.h>

#include "context.h"

// Инициализация общего контекста, мьютекса модели
// и очереди вызовов
int initializeSimulationContext(
    SimulationContext *context,
    Person *people,
    int peopleCount,
    Lift *lifts,
    int liftCount,
    const SimulationConfig *config
) {
    context->people = people;
    context->peopleCount = peopleCount;
    context->lifts = lifts;
    context->liftCount = liftCount;
    context->config = config;
    context->stopRequested = 0;
    context->currentTime = 0;

    int errorCode = pthread_mutex_init(&context->modelMutex, NULL);
    if (errorCode != 0) {
        fprintf(
            stderr,
            "Error initializing model mutex: %s\n",
            strerror(errorCode)
        );
        return 0;
    }

    errorCode = pthread_cond_init(&context->clockCondition, NULL);
    if (errorCode != 0) {
        fprintf(
            stderr,
            "Error initializing clock condition: %s\n",
            strerror(errorCode)
        );
        pthread_mutex_destroy(&context->modelMutex);
        return 0;
    }

    if (!initializeCallQueue(&context->callQueue, peopleCount)) {
        pthread_cond_destroy(&context->clockCondition);
        pthread_mutex_destroy(&context->modelMutex);
        return 0;
    }

    return 1;
}

// Освобождение ресурсов контекста после завершения потоков
void destroySimulationContext(SimulationContext *context) {
    destroyCallQueue(&context->callQueue);

    int errorCode = pthread_cond_destroy(&context->clockCondition);
    if (errorCode != 0) {
        fprintf(
            stderr,
            "Error destroying clock condition: %s\n",
            strerror(errorCode)
        );
    }

    errorCode = pthread_mutex_destroy(&context->modelMutex);
    if (errorCode != 0) {
        fprintf(
            stderr,
            "Error destroying model mutex: %s\n",
            strerror(errorCode)
        );
    }
}

// Передача потокам команды завершить работу
void requestSimulationStop(SimulationContext *context) {
    pthread_mutex_lock(&context->modelMutex);

    context->stopRequested = 1;
    pthread_cond_broadcast(&context->clockCondition);

    pthread_mutex_unlock(&context->modelMutex);

    wakeCallQueue(&context->callQueue);
}

// Проверка команды завершения
int isSimulationStopRequested(SimulationContext *context) {
    int stopRequested;

    pthread_mutex_lock(&context->modelMutex);
    stopRequested = context->stopRequested;
    pthread_mutex_unlock(&context->modelMutex);

    return stopRequested;
}

// Изменение времени и пробуждение ожидающих потоков
void setSimulationTime(
    SimulationContext *context,
    int currentTime
) {
    pthread_mutex_lock(&context->modelMutex);

    context->currentTime = currentTime;
    pthread_cond_broadcast(&context->clockCondition);

    pthread_mutex_unlock(&context->modelMutex);
}

// Ожидание времени появления пасажира
int waitForSimulationTime(
    SimulationContext *context,
    int targetTime
) {
    pthread_mutex_lock(&context->modelMutex);

    while (context->currentTime < targetTime &&
           !context->stopRequested) {
        pthread_cond_wait(
            &context->clockCondition,
            &context->modelMutex
        );
    }

    int shouldContinue = !context->stopRequested;

    pthread_mutex_unlock(&context->modelMutex);
    return shouldContinue;
}
