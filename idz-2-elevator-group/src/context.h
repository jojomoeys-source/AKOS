#ifndef CONTEXT_H
#define CONTEXT_H

#include <pthread.h>

#include "call_queue.h"
#include "config.h"
#include "lift.h"
#include "person.h"

typedef struct {
    // Общие параметры
    Person *people;
    int peopleCount;
    Lift *lifts;
    int liftCount;
    const SimulationConfig *config;

    CallQueue callQueue;        // Ожидающие обработки вызовы

    // Защита состояний пассажиров и лифтов
    pthread_mutex_t modelMutex;

    // Текущее время и ожидание следуюющего такта
    int currentTime;
    pthread_cond_t clockCondition;

    // Количество лифтов завершивших текущий такт
    int completedLiftCount;
    pthread_cond_t liftTickCondition;

    // Флаг завершения работы потоков. Доступ только через modelMutex
    int stopRequested;
} SimulationContext;

int initializeSimulationContext(
    SimulationContext *context,
    Person *people,
    int peopleCount,
    Lift *lifts,
    int liftCount,
    const SimulationConfig *config
);

void destroySimulationContext(SimulationContext *context);
void requestSimulationStop(SimulationContext *context);
int isSimulationStopRequested(SimulationContext *context);

void setSimulationTime(SimulationContext *context, int currentTime);

int waitForSimulationTime(SimulationContext *context, int targetTime);

void completeLiftTick(SimulationContext *context);
int waitForAllLifts(SimulationContext *context);

#endif
