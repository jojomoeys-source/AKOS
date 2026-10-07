#include "actions.h"
#include "dispatcher.h"
#include "interruption.h"
#include "logger.h"
#include "service.h"
#include "simulation.h"
#include "statistics.h"

// Добавление пассажиров, время появления которых уже наступило
void processArrivals(Person *people, int peopleCount, int currentTime) {
    for (int i = 0; i < peopleCount; i++) {
        if (people[i].state == PERSON_NOT_ARRIVED &&
            people[i].arrivalTime <= currentTime) {
            people[i].state = PERSON_WAITING;

            logEvent("[Time %d] Person %d appeared on floor %d "
                     "and wants to go to floor %d.\n",
                     currentTime,
                     people[i].id,
                     people[i].startFloor,
                     people[i].targetFloor);
            logEvent("[Time %d] Call %d was registered: floor %d, "
                     "direction %s.\n",
                     currentTime,
                     people[i].id,
                     people[i].startFloor,
                     people[i].targetFloor > people[i].startFloor
                         ? "up" : "down");
        }
    }
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
    int currentTime = 0;
    while (!isInterruptRequested() &&
           !allPeopleDelivered(people, peopleCount) &&
           (config->workPeriod == 0 || currentTime < config->workPeriod)) {
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
        processArrivals(people, peopleCount, currentTime);
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

        if (allPeopleDelivered(people, peopleCount)) {
            logEvent(
                "[Time %d] Simulation finished. All people delivered.\n",
                currentTime
            );
            printSimulationSummary(
                people, peopleCount, lifts, liftCount, currentTime
            );
            return;
        }

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
    printSimulationSummary(
        people, peopleCount, lifts, liftCount, currentTime
    );
}
