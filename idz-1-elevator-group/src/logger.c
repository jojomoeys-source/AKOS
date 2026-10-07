#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include "logger.h"

static int logFileDescriptor = -1;

// Открытие файла для записи событий
int openLog(const char *filePath) {
    logFileDescriptor = open(
        filePath,
        O_WRONLY | O_CREAT | O_TRUNC,
        0644
    );

    if (logFileDescriptor == -1) {
        perror("Error opening log file");
        return 0;
    }

    return 1;
}

// Вывод сообщения в терминал и в лог
void logEvent(const char *format, ...) {
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
}

// Закрытие файла с логом
void closeLog(void) {
    if (logFileDescriptor != -1) {
        close(logFileDescriptor);
        logFileDescriptor = -1;
    }
}
