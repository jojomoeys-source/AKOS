#include "context.h"
#include "dispatcher_thread.h"

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

        /*
         * Здесь следующим шагом диспетчер
         * назначит вызов одному из лифтов.
         */
    }

    return NULL;
}
