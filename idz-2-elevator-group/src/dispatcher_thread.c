#include "context.h"
#include "dispatcher.h"
#include "dispatcher_thread.h"
#include "logger.h"

// Попытка обработать один вызов
static int tryAssignCall(
    SimulationContext *context,
    const Call *call,
    int *retryTime
) {
    pthread_mutex_lock(&context->modelMutex);

    if (call->personIndex < 0 ||
        call->personIndex >= context->peopleCount) {
        pthread_mutex_unlock(&context->modelMutex);
        return 1;
    }

    Person *person = &context->people[call->personIndex];

    // Вызов уже не требует обработки
    if (person->state != PERSON_WAITING ||
        person->assignedLift != -1) {
        pthread_mutex_unlock(&context->modelMutex);
        return 1;
    }

    int assigned = assignLiftToPerson(
        context->lifts,
        context->liftCount,
        person,
        context->currentTime,
        context->config->strategy,
        context->config->doorOpeningTime
    );

    *retryTime = context->currentTime + 1;

    pthread_mutex_unlock(&context->modelMutex);
    return assigned;
}

// Получение и обработка вызовов пасажиров
void *dispatcherThreadMain(void *argument) {
    SimulationContext *context = argument;

    while (!isSimulationStopRequested(context)) {
        Call call;

        if (!dequeueCall(&context->callQueue, &call)) {
            continue;
        }

        if (isSimulationStopRequested(context)) {
            break;
        }

        int retryTime;

        if (tryAssignCall(context, &call, &retryTime)) {
            continue;
        }

        // Cвободного лифта нет. Диспетчер ждёт следующий такт и возвращает вызов в очередь.
        if (!waitForSimulationTime(context, retryTime)) {
            break;
        }

        if (!enqueueCall(&context->callQueue, &call)) {
            logEvent(
                "Error: dispatcher could not return call "
                "to the queue.\n"
            );
        }
    }

    return NULL;
}
