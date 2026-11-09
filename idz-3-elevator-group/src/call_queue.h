#ifndef CALL_QUEUE_H
#define CALL_QUEUE_H

#include <pthread.h>
#include <semaphore.h>

#include "lift.h"

typedef struct {
    int personIndex;       // Индекс пассажира в общей модели
    int startFloor;        // Этаж, с которого поступил вызов
    Direction direction;   // Требуемое направление движения
} Call;

typedef struct {
    Call *items;
    int capacity;
    int head;
    int tail;
    int count;

    // Защищает внутренние поля очереди
    pthread_mutex_t mutex;

    // Количество вызовов, доступных диспетчеру
    sem_t *availableCalls;
} CallQueue;

int initializeCallQueue(CallQueue *queue, int capacity);
void destroyCallQueue(CallQueue *queue);

int enqueueCall(CallQueue *queue, const Call *call);
int dequeueCall(CallQueue *queue, Call *call);
void wakeCallQueue(CallQueue *queue);

#endif
