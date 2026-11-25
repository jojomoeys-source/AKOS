#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "call_queue.h"

// Создание пустой очереди вызовов
int initializeCallQueue(CallQueue *queue, int capacity) {
    queue->availableCalls = SEM_FAILED;
    queue->items = calloc((size_t)capacity, sizeof(Call));
    if (queue->items == NULL) {
        fprintf(stderr, "Error allocating call queue.\n");
        return 0;
    }

    queue->capacity = capacity;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;

    int errorCode = pthread_mutex_init(&queue->mutex, NULL);
    if (errorCode != 0) {
        fprintf(
            stderr,
            "Error initializing call queue mutex: %s\n",
            strerror(errorCode)
        );
        free(queue->items);
        queue->items = NULL;
        return 0;
    }

    char semaphoreName[64];
    snprintf(
        semaphoreName,
        sizeof(semaphoreName),
        "/elevator_calls_%ld",
        (long)getpid()
    );

    // Удаляем старый объект после возможного
    // аварийного запуска
    sem_unlink(semaphoreName);

    queue->availableCalls = sem_open(
        semaphoreName,
        O_CREAT | O_EXCL,
        0600,
        0
    );
    if (queue->availableCalls == SEM_FAILED) {
        perror("Error initializing call queue semaphore");
        pthread_mutex_destroy(&queue->mutex);
        free(queue->items);
        queue->items = NULL;
        return 0;
    }

    // После удаления имени семафор работает
    // до вызова sem_close
    sem_unlink(semaphoreName);

    return 1;
}

// Освобождение ресурсов очереди
void destroyCallQueue(CallQueue *queue) {
    if (queue->availableCalls != SEM_FAILED) {
        sem_close(queue->availableCalls);
        queue->availableCalls = SEM_FAILED;
    }

    pthread_mutex_destroy(&queue->mutex);

    free(queue->items);
    queue->items = NULL;
}

// Добавление вызова пассажиром
int enqueueCall(CallQueue *queue, const Call *call) {
    pthread_mutex_lock(&queue->mutex);

    if (queue->count >= queue->capacity) {
        pthread_mutex_unlock(&queue->mutex);
        return 0;
    }

    queue->items[queue->tail] = *call;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;

    pthread_mutex_unlock(&queue->mutex);

    sem_post(queue->availableCalls);
    return 1;
}

// Получение следующего вызова диспетчером
int dequeueCall(CallQueue *queue, Call *call) {
    int waitResult;

    do {
        waitResult = sem_wait(queue->availableCalls);
    } while (waitResult == -1 && errno == EINTR);

    if (waitResult == -1) {
        perror("Error waiting for a call");
        return 0;
    }

    pthread_mutex_lock(&queue->mutex);

    if (queue->count == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return 0;
    }

    *call = queue->items[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;

    pthread_mutex_unlock(&queue->mutex);
    return 1;
}

// Пробуждение диспетчера при завершении программы
void wakeCallQueue(CallQueue *queue) {
    sem_post(queue->availableCalls);
}
