#include "shared_queue.h"

#include <errno.h>
#include <stdio.h>

static int waitForSemaphore(sem_t *semaphore, const char *operationName) {
    while (sem_wait(semaphore) == -1) {
        if (errno == EINTR) {
            continue;
        }

        perror(operationName);
        return -1;
    }

    return 0;
}

int enqueueSharedCall(SharedState *state, IpcSync *sync, const Call *call) {
    // Добавление вызова в общую очередь и пробуждение диспетчера
    if (state == NULL || sync == NULL || call == NULL ||
        sync->stateLock == SEM_FAILED ||
        sync->callsAvailable == SEM_FAILED) {
        return -1;
    }

    if (waitForSemaphore(sync->stateLock, "sem_wait stateLock") == -1) {
        return -1;
    }

    SharedCallQueue *queue = &state->callQueue;

    if (queue->count < 0 || queue->count >= MAX_CALLS ||
        queue->tail < 0 || queue->tail >= MAX_CALLS) {
        sem_post(sync->stateLock);
        return -1;
    }

    queue->items[queue->tail] = *call;
    queue->tail = (queue->tail + 1) % MAX_CALLS;
    queue->count++;

    if (sem_post(sync->callsAvailable) == -1) {
        perror("sem_post callsAvailable");

        queue->tail = (queue->tail + MAX_CALLS - 1) % MAX_CALLS;
        queue->count--;

        sem_post(sync->stateLock);
        return -1;
    }

    if (sem_post(sync->stateLock) == -1) {
        perror("sem_post stateLock");
        return -1;
    }

    return 0;
}

int dequeueSharedCall(SharedState *state, IpcSync *sync, Call *call) {
    // Получение первого вызова из общей очереди
    if (state == NULL || sync == NULL || call == NULL ||
        sync->stateLock == SEM_FAILED ||
        sync->callsAvailable == SEM_FAILED) {
        return -1;
    }

    if (waitForSemaphore(
            sync->callsAvailable,
            "sem_wait callsAvailable") == -1) {
        return -1;
    }

    if (waitForSemaphore(sync->stateLock, "sem_wait stateLock") == -1) {
        return -1;
    }

    SharedCallQueue *queue = &state->callQueue;

    if (queue->count <= 0 ||
        queue->head < 0 || queue->head >= MAX_CALLS) {
        sem_post(sync->stateLock);
        return -1;
    }

    *call = queue->items[queue->head];
    queue->head = (queue->head + 1) % MAX_CALLS;
    queue->count--;

    if (sem_post(sync->stateLock) == -1) {
        perror("sem_post stateLock");
        return -1;
    }

    return 0;
}
