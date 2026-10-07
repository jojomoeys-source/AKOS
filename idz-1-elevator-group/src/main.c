#include <stdio.h>

#include "input.h"
#include "interruption.h"
#include "logger.h"
#include "options.h"
#include "simulation.h"

// Точка входа в программу
int main(int argumentCount, char **argumentValues) {
    SimulationData data = {0};
    ProgramOptions options;
    FILE *inputStream = stdin;
    int showPrompts = 1;

    // Сначала разбираем параметры запуска
    OptionsStatus optionsStatus = parseProgramOptions(
        argumentCount,
        argumentValues,
        &options
    );

    if (optionsStatus == OPTIONS_HELP) {
        return 0;
    }
    if (optionsStatus == OPTIONS_ERROR) {
        return 1;
    }

    // При наличии --config читаем данные не из терминала, а из файла
    if (options.configPath != NULL) {
        inputStream = fopen(options.configPath, "r");
        if (inputStream == NULL) {
            perror("Error opening configuration file");
            return 1;
        }
        showPrompts = 0;
    }

    if (!openLog(options.logPath)) {
        if (inputStream != stdin) {
            fclose(inputStream);
        }
        return 1;
    }

    if (!readSimulationData(&data, inputStream, showPrompts)) {
        if (inputStream != stdin) {
            fclose(inputStream);
        }
        closeLog();
        return 1;
    }

    if (inputStream != stdin) {
        fclose(inputStream);
    }

    // После чтения данных настраиваем корректное завершение по Ctrl+C
    if (!setupInterruptHandler()) {
        freeSimulationData(&data);
        closeLog();
        return 1;
    }

    // Запуск основного цикла моделирования
    runSimulation(
        data.people,
        data.config.peopleCount,
        data.lifts,
        data.config.liftCount,
        &data.config
    );
    freeSimulationData(&data);
    closeLog();

    return 0;
}
