#include <stdio.h>
#include <string.h>

#include "options.h"

// Вывод подсказки по запуску
void printUsage(const char *programName) {
    printf("Usage: %s [--config FILE] [--log FILE]\n", programName);
    printf("       %s --help\n", programName);
    printf("\nOptions:\n");
    printf("  --config FILE  Read simulation data from FILE.\n");
    printf("  --log FILE     Write events to FILE (default: elevator.log).\n");
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

    for (int i = 1; i < argumentCount; i++) {
        if (strcmp(argumentValues[i], "--help") == 0) {
            printUsage(argumentValues[0]);
            return OPTIONS_HELP;
        }

        if (strcmp(argumentValues[i], "--config") == 0 ||
            strcmp(argumentValues[i], "--log") == 0) {
            if (i + 1 >= argumentCount) {
                fprintf(stderr, "Error: %s requires a file path.\n",
                        argumentValues[i]);
                return OPTIONS_ERROR;
            }

            if (strcmp(argumentValues[i], "--config") == 0) {
                options->configPath = argumentValues[++i];
            } else {
                options->logPath = argumentValues[++i];
            }
            continue;
        }

        fprintf(stderr, "Error: unknown option '%s'.\n", argumentValues[i]);
        printUsage(argumentValues[0]);
        return OPTIONS_ERROR;
    }

    return OPTIONS_OK;
}
