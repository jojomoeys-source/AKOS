#ifndef CONFIG_H
#define CONFIG_H

typedef enum {
    STRATEGY_NEAREST_IDLE = 1,
    STRATEGY_ON_THE_WAY = 2
} DispatchStrategy;

typedef struct {
    // Основные размеры модели
    int floors;                  // Количество этажей
    int liftCount;               // Количество лифтов
    int peopleCount;             // Количество пассажиров
    // Настройки появления пассажиров
    int minArrivalInterval;      // Минимальный интервал появления
    int maxArrivalInterval;      // Максимальный интервал появления
    // Время в программе измеряется тактами
    int movementSpeed;           // Скорость в этажах за такт
    int doorOpeningTime;         // Время открытия дверей
    int boardingTime;            // Время посадки одного пассажира
    int unloadingTime;           // Время высадки одного пассажира
    int workPeriod;              // Ноль означает работу до конца
    DispatchStrategy strategy;   // Выбранная стратегия диспетчера
} SimulationConfig;

#endif
