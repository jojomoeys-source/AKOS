#ifndef OPTIONS_H
#define OPTIONS_H

typedef struct {
    const char *configPath;       // Файл с исходными данными
    const char *logPath;          // Файл для записи событий
    const char *hostAddress;      // Адрес привязки TCP-сервера
    const char *port;             // Порт TCP-сервера
    int clientWaitMilliseconds;   // Время для подключения наблюдателей
    int useUnnamedSemaphores;
} ProgramOptions;

typedef enum {
    OPTIONS_OK,
    OPTIONS_HELP,
    OPTIONS_ERROR
} OptionsStatus;

OptionsStatus parseProgramOptions(
    int argumentCount,
    char **argumentValues,
    ProgramOptions *options
);

void printUsage(const char *programName);

#endif
