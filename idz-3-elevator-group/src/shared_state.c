#define _POSIX_C_SOURCE 200809L

#include "shared_state.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

SharedState *createSharedState(const char *name, int *fd, size_t *size) {
    // Создание POSIX shared memory для состояния модели
    if (name == NULL || fd == NULL || size == NULL) {
        return NULL;
    }

    *fd = -1;
    *size = sizeof(SharedState);

    *fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (*fd == -1) {
        perror("shm_open");
        return NULL;
    }

    if (ftruncate(*fd, (off_t)*size) == -1) {
        perror("ftruncate");
        close(*fd);
        *fd = -1;
        shm_unlink(name);
        return NULL;
    }

    void *mapping = mmap(
        NULL,
        *size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        *fd,
        0
    );
    if (mapping == MAP_FAILED) {
        perror("mmap");
        close(*fd);
        *fd = -1;
        shm_unlink(name);
        return NULL;
    }

    SharedState *state = mapping;
    memset(state, 0, *size);

    state->version = 1;
    state->status = SIMULATION_INITIALIZING;
    state->stopReason = STOP_REASON_NONE;
    state->initialized = 0;

    return state;
}

SharedState *openSharedState(const char *name, int *fd, size_t *size) {
    // Подключение процесса к уже созданной общей памяти
    if (name == NULL || fd == NULL || size == NULL) {
        return NULL;
    }

    *fd = -1;
    *size = 0;

    int sharedMemoryFd = shm_open(name, O_RDWR, 0);
    if (sharedMemoryFd == -1) {
        perror("shm_open existing shared state");
        return NULL;
    }

    struct stat fileStatus;
    if (fstat(sharedMemoryFd, &fileStatus) == -1) {
        perror("fstat shared state");
        close(sharedMemoryFd);
        return NULL;
    }

    if (fileStatus.st_size < (off_t)sizeof(SharedState)) {
        fprintf(stderr, "Shared memory object is smaller than SharedState.\n");
        close(sharedMemoryFd);
        return NULL;
    }

    size_t sharedMemorySize = (size_t)fileStatus.st_size;
    void *mapping = mmap(
        NULL,
        sharedMemorySize,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        sharedMemoryFd,
        0
    );
    if (mapping == MAP_FAILED) {
        perror("mmap existing shared state");
        close(sharedMemoryFd);
        return NULL;
    }

    SharedState *state = mapping;
    if (state->version != 1 || state->initialized != 1) {
        fprintf(stderr, "Shared state is uninitialized or has an unsupported version.\n");
        munmap(mapping, sharedMemorySize);
        close(sharedMemoryFd);
        return NULL;
    }

    *fd = sharedMemoryFd;
    *size = sharedMemorySize;
    return state;
}

void closeSharedState(SharedState *state, int fd, size_t size) {
    if (state != NULL && munmap(state, size) == -1) {
        perror("munmap");
    }

    if (fd >= 0 && close(fd) == -1) {
        perror("close");
    }
}

int removeSharedState(const char *name) {
    if (name == NULL) {
        return -1;
    }

    return shm_unlink(name);
}
