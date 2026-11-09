#include "actions.h"
#include "lift_thread.h"
#include "service.h"

// Выполнение одного такта конкретным лифтом
static void processLiftTick(
    SimulationContext *context,
    Lift *lift,
    int currentTime
) {
    if (currentTime > 0) {
        advanceTimedLiftActions(
            lift,
            1,
            context->people,
            context->peopleCount,
            currentTime
        );

        for (int step = 0;
             step < context->config->movementSpeed;
             step++) {
            moveLifts(lift, 1, currentTime);

            checkStopsAtCurrentFloor(
                lift,
                1,
                context->people,
                context->peopleCount,
                currentTime,
                context->config->doorOpeningTime
            );
        }
    }

    serviceLiftsAtCurrentFloor(
        lift,
        1,
        context->people,
        context->peopleCount,
        currentTime,
        context->config
    );
}

// Последавательная обработка тактов одним лифтом
void *liftThreadMain(void *argument) {
    LiftThreadArguments *arguments = argument;
    SimulationContext *context = arguments->context;
    int liftIndex = arguments->liftIndex;
    int nextTime = 0;

    while (waitForSimulationTime(context, nextTime)) {
        pthread_mutex_lock(&context->modelMutex);

        if (context->stopRequested) {
            pthread_mutex_unlock(&context->modelMutex);
            break;
        }

        // Основной поток может успеть изменить время
        // несколько раз. Лифт обрабатывает все пропущенные такты последовательно
        while (nextTime <= context->currentTime) {
            processLiftTick(
                context,
                &context->lifts[liftIndex],
                nextTime
            );
            nextTime++;
        }

        pthread_mutex_unlock(&context->modelMutex);
        completeLiftTick(context);
    }

    return NULL;
}
