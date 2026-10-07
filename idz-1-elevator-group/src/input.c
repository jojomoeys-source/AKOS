#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "input.h"

// Получение случайного числа между minimum и maximum
static int randomInRange(int minimum, int maximum) {
    return minimum + rand() % (maximum - minimum + 1);
}

// Чтение положительного целого числа
static int readPositiveParameter(
    FILE *stream,
    int showPrompts,
    const char *prompt,
    int *value
) {
    if (showPrompts) {
        printf("%s", prompt);
    }
    if (fscanf(stream, "%d", value) != 1 || *value < 1) {
        printf("Error: value must be a positive integer.\n");
        return 0;
    }
    return 1;
}

// Чтение всех исходных данных для симуляции
int readSimulationData(
    SimulationData *data,
    FILE *inputStream,
    int showPrompts
) {
    SimulationConfig *config = &data->config;

    if (!readPositiveParameter(inputStream, showPrompts,
            "Enter the number of floors: ", &config->floors) ||
        config->floors < 2) {
        printf("Error: the number of floors must be at least 2.\n");
        return 0;
    }
    if (!readPositiveParameter(inputStream, showPrompts,
            "Enter the number of lifts: ", &config->liftCount) ||
        !readPositiveParameter(inputStream, showPrompts,
            "Enter the total number of people: ", &config->peopleCount)) {
        return 0;
    }

    // Создаём лифты и заполняем их начальные данные
    data->lifts = malloc((size_t)config->liftCount * sizeof(Lift));
    if (data->lifts == NULL) {
        printf("Error: memory allocation failed for lifts.\n");
        return 0;
    }

    for (int i = 0; i < config->liftCount; i++) {
        int capacity;
        int initialFloor;

        if (showPrompts) {
            printf("Enter capacity and initial floor of lift %d: ", i + 1);
        }
        if (fscanf(inputStream, "%d %d", &capacity, &initialFloor) != 2 ||
            capacity < 1 || initialFloor < 1 ||
            initialFloor > config->floors) {
            printf("Error: invalid parameters for lift %d.\n", i + 1);
            freeSimulationData(data);
            return 0;
        }
        initializeLift(&data->lifts[i], i + 1, capacity, initialFloor);
    }

    // Диапазон нужен для случайного времени появления пассажиров
    if (showPrompts) {
        printf("Enter minimum and maximum passenger arrival interval: ");
    }
    if (fscanf(inputStream, "%d %d", &config->minArrivalInterval,
               &config->maxArrivalInterval) != 2 ||
        config->minArrivalInterval < 0 ||
        config->maxArrivalInterval < config->minArrivalInterval) {
        printf("Error: invalid passenger arrival interval range.\n");
        freeSimulationData(data);
        return 0;
    }

    // Считываем скорость и длительность всех действий
    if (!readPositiveParameter(inputStream, showPrompts,
            "Enter lift speed (floors per tick): ",
            &config->movementSpeed) ||
        !readPositiveParameter(inputStream, showPrompts,
            "Enter door opening time (ticks): ",
            &config->doorOpeningTime) ||
        !readPositiveParameter(inputStream, showPrompts,
            "Enter boarding time per passenger (ticks): ",
            &config->boardingTime) ||
        !readPositiveParameter(inputStream, showPrompts,
            "Enter unloading time per passenger (ticks): ",
            &config->unloadingTime)) {
        freeSimulationData(data);
        return 0;
    }

    if (showPrompts) {
        printf("Enter dispatch strategy (1 - nearest idle, "
               "2 - on-the-way then nearest idle): ");
    }
    int strategy;
    if (fscanf(inputStream, "%d", &strategy) != 1 ||
        (strategy != STRATEGY_NEAREST_IDLE &&
         strategy != STRATEGY_ON_THE_WAY)) {
        printf("Error: dispatch strategy must be 1 or 2.\n");
        freeSimulationData(data);
        return 0;
    }
    config->strategy = (DispatchStrategy)strategy;

    if (showPrompts) {
        printf("Enter work period in ticks (0 - until all are delivered): ");
    }
    if (fscanf(inputStream, "%d", &config->workPeriod) != 1 ||
        config->workPeriod < 0) {
        printf("Error: work period cannot be negative.\n");
        freeSimulationData(data);
        return 0;
    }

    // Создаём пассажиров и постепенно накапливаем время их появления
    data->people = malloc((size_t)config->peopleCount * sizeof(Person));
    if (data->people == NULL) {
        printf("Error: memory allocation failed for people.\n");
        freeSimulationData(data);
        return 0;
    }

    srand((unsigned int)time(NULL));
    int arrivalTime = 0;
    for (int i = 0; i < config->peopleCount; i++) {
        int startFloor;
        int targetFloor;

        if (showPrompts) {
            printf("Enter start and target floor for person %d: ", i + 1);
        }
        if (fscanf(inputStream, "%d %d", &startFloor, &targetFloor) != 2 ||
            startFloor < 1 || startFloor > config->floors ||
            targetFloor < 1 || targetFloor > config->floors ||
            startFloor == targetFloor) {
            printf("Error: invalid route for person %d.\n", i + 1);
            freeSimulationData(data);
            return 0;
        }

        arrivalTime += randomInRange(
            config->minArrivalInterval,
            config->maxArrivalInterval
        );
        initializePerson(&data->people[i], i + 1, startFloor,
                         targetFloor, arrivalTime);
    }

    return 1;
}

// Освобождение памяти, выделенной под лифты и пассажиров
void freeSimulationData(SimulationData *data) {
    free(data->people);
    free(data->lifts);
    data->people = NULL;
    data->lifts = NULL;
}
