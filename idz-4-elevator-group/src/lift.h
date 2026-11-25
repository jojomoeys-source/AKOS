#ifndef LIFT_H
#define LIFT_H

typedef enum {
    DIRECTION_DOWN = -1,  // Движение вниз
    DIRECTION_IDLE = 0,   // Лифт стоит
    DIRECTION_UP = 1      // Движение вверх
} Direction;

typedef enum {
    LIFT_IDLE,            // Свободен
    LIFT_MOVING,          // Движется между этажами
    LIFT_DOORS_OPENING,   // Открывает двери
    LIFT_UNLOADING,       // Выпускает пассажира
    LIFT_BOARDING,        // Принимает пассажира
    LIFT_DOORS_OPEN       // Стоит с открытыми дверями
} LiftState;

typedef struct {
    // Текущее положение и маршрут лифта
    int id;                    // Номер лифта
    int currentFloor;          // Текущий этаж
    int targetFloor;           // Следующая остановка
    int capacity;              // Вместимость кабины
    int peopleInside;          // Пассажиров внутри
    int reservedPlaces;        // Места для назначенных пассажиров
    // Статистика и оставшееся время действия
    int floorsTravelled;       // Пройдено этажей
    int stopCount;             // Сделано остановок
    int actionTimeLeft;        // Осталось тактов до конца действия
    int activePersonIndex;     // Пассажир, который входит или выходит
    Direction direction;       // Направление движения
    LiftState state;           // Текущее состояние
} Lift;

void initializeLift(Lift *lift, int id, int capacity, int initialFloor);
void moveLifts(Lift *lifts, int liftCount, int currentTime);

#endif
