#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>

#include "interruption.h"

static volatile sig_atomic_t interruptRequested = 0;

// Запоминаем нажатие Ctrl+C, чтобы спокойно закончить текущий такт
static void handleInterrupt(int signalNumber) {
    (void)signalNumber;
    interruptRequested = 1;
}

// Настройка обработки Ctrl+C
int setupInterruptHandler(void) {
    struct sigaction action = {0};

    action.sa_handler = handleInterrupt;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(SIGINT, &action, NULL) == -1) {
        perror("Error installing SIGINT handler");
        return 0;
    }

    return 1;
}

// Проверка запроса на остановку программы
int isInterruptRequested(void) {
    return interruptRequested != 0;
}
