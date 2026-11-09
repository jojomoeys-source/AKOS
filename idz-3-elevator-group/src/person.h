#ifndef PERSON_H
#define PERSON_H

#include <stdbool.h>

typedef enum {
    PERSON_NOT_ARRIVED,  // Ещё не появился
    PERSON_WAITING,      // Ожидает на этаже
    PERSON_IN_LIFT,      // Находится в кабине
    PERSON_DELIVERED     // Доставлен на нужный этаж
} PersonState;

typedef struct {
    // У пассажира может быть только один назначенный лифт
    int id;                 // Номер пассажира
    int startFloor;         // Этаж отправления
    int targetFloor;        // Этаж назначения
    int arrivalTime;        // Время появления
    int assignedLift;       // Номер назначенного лифта
    PersonState state;      // Текущее состояние
} Person;

void initializePerson(
    Person *person,
    int id,
    int startFloor,
    int targetFloor,
    int arrivalTime
);
bool allPeopleDelivered(Person *personArray, int totalPeople);

#endif
