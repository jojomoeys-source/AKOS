#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "options.h"

// Вывод подсказки по запуску
void printUsage(const char *programName) {
    printf("Usage: %s [--config FILE] [--log FILE] [--host ADDRESS] "
           "[--port PORT] [--start-delay MS] [--unnamed-semaphores]\n",
           programName);
    printf("       %s --help\n", programName);
    printf("\nOptions:\n");
    printf("  --config FILE  Read simulation data from FILE.\n");
    printf("  --log FILE     Write events to FILE (default: elevator.log).\n");
    printf("  --host ADDRESS TCP bind address (default: 127.0.0.1).\n");
    printf("                        Use 0.0.0.0 to accept remote monitors.\n");
    printf("  --port PORT    TCP port for observer clients (default: 5050).\n");
    printf("  --start-delay MS  Wait before the first tick (default: 3000).\n");
    printf("  --unnamed-semaphores  Use process-shared unnamed semaphores.\n");
    printf("  --help         Show this help.\n");
}

// Разбор параметров, переданных при запуске
OptionsStatus parseProgramOptions(
    int argumentCount,
    char **argumentValues,
    ProgramOptions *options
) {
    options->configPath = NULL;
    options->logPath = "elevator.log";
    options->hostAddress = "127.0.0.1";
    options->port = "5050";
    options->clientWaitMilliseconds = 3000;
    options->useUnnamedSemaphores = 0;

    for (int i = 1; i < argumentCount; i++) {
        if (strcmp(argumentValues[i], "--help") == 0) {
            printUsage(argumentValues[0]);
            return OPTIONS_HELP;
        }

        if (strcmp(argumentValues[i], "--unnamed-semaphores") == 0) {
            options->useUnnamedSemaphores = 1;
            continue;
        }

        if (strcmp(argumentValues[i], "--config") == 0 ||
            strcmp(argumentValues[i], "--log") == 0 ||
            strcmp(argumentValues[i], "--host") == 0 ||
            strcmp(argumentValues[i], "--port") == 0 ||
            strcmp(argumentValues[i], "--start-delay") == 0) {
            if (i + 1 >= argumentCount) {
                fprintf(stderr, "Error: %s requires a file path.\n",
                        argumentValues[i]);
                return OPTIONS_ERROR;
            }

            if (strcmp(argumentValues[i], "--config") == 0) {
                options->configPath = argumentValues[++i];
            } else if (strcmp(argumentValues[i], "--log") == 0) {
                options->logPath = argumentValues[++i];
            } else if (strcmp(argumentValues[i], "--host") == 0) {
                options->hostAddress = argumentValues[++i];
            } else if (strcmp(argumentValues[i], "--start-delay") == 0) {
                char *end = NULL;
                errno = 0;
                long milliseconds = strtol(argumentValues[++i], &end, 10);
                if (errno != 0 || end == argumentValues[i] || *end != '\0' ||
                    milliseconds < 0 || milliseconds > 60000 ||
                    milliseconds > INT_MAX) {
                    fprintf(stderr, "Error: --start-delay must be from 0 to 60000 milliseconds.\n");
                    return OPTIONS_ERROR;
                }
                options->clientWaitMilliseconds = (int)milliseconds;
            } else {
                options->port = argumentValues[++i];
            }
            continue;
        }

        fprintf(stderr, "Error: unknown option '%s'.\n", argumentValues[i]);
        printUsage(argumentValues[0]);
        return OPTIONS_ERROR;
    }

    return OPTIONS_OK;
}
