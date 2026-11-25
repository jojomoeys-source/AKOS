#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include "logger.h"

static int logFileDescriptor = -1;
static int eventPipeDescriptor = -1;

// Не позволяет потокам одновременно писать
// в терминал и журнал
static pthread_mutex_t loggerMutex = PTHREAD_MUTEX_INITIALIZER;

// Открытие файла для записи событий
static int openLogWithFlags(const char *filePath, int flags) {
    logFileDescriptor = open(
        filePath,
        O_WRONLY | O_CREAT | flags,
        0644
    );

    if (logFileDescriptor == -1) {
        perror("Error opening log file");
        return 0;
    }

    return 1;
}

int openLog(const char *filePath) {
    return openLogWithFlags(filePath, O_TRUNC);
}

int openLogAppend(const char *filePath) {
    return openLogWithFlags(filePath, O_APPEND);
}

// Вывод сообщения в терминал и в лог
void logEvent(const char *format, ...) {
    pthread_mutex_lock(&loggerMutex);

    char message[2048];
    va_list arguments;
    va_start(arguments, format);
    int messageLength = vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);

    if (messageLength < 0) {
        pthread_mutex_unlock(&loggerMutex);
        return;
    }

    size_t bytesToWrite = (size_t)messageLength;
    if (bytesToWrite >= sizeof(message)) {
        bytesToWrite = sizeof(message) - 1;
    }

    (void)write(STDOUT_FILENO, message, bytesToWrite);
    if (logFileDescriptor != -1) {
        (void)write(logFileDescriptor, message, bytesToWrite);
    }
    if (eventPipeDescriptor != -1) {
        size_t offset = 0;
        while (offset < bytesToWrite) {
            ssize_t written = write(
                eventPipeDescriptor,
                message + offset,
                bytesToWrite - offset
            );
            if (written <= 0) {
                break;
            }
            offset += (size_t)written;
        }
    }

    pthread_mutex_unlock(&loggerMutex);
}

void setEventPipe(int descriptor) {
    eventPipeDescriptor = descriptor;
}

// Закрытие файла с логом
void closeLog(void) {
    if (logFileDescriptor != -1) {
        close(logFileDescriptor);
        logFileDescriptor = -1;
    }
}
