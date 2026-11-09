#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include "logger.h"

static int logFileDescriptor = -1;

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

    va_list terminalArguments;
    va_list fileArguments;

    va_start(terminalArguments, format);
    va_copy(fileArguments, terminalArguments);

    vdprintf(STDOUT_FILENO, format, terminalArguments);
    if (logFileDescriptor != -1) {
        vdprintf(logFileDescriptor, format, fileArguments);
    }

    va_end(fileArguments);
    va_end(terminalArguments);

    pthread_mutex_unlock(&loggerMutex);
}

// Закрытие файла с логом
void closeLog(void) {
    if (logFileDescriptor != -1) {
        close(logFileDescriptor);
        logFileDescriptor = -1;
    }
}
