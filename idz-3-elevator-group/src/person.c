#include "person.h"

// Создание пассажира с заданным маршрутом
void initializePerson(
    Person *person,
    int id,
    int startFloor,
    int targetFloor,
    int arrivalTime
) {
    person->id = id;
    person->startFloor = startFloor;
    person->targetFloor = targetFloor;
    person->arrivalTime = arrivalTime;
    person->assignedLift = -1;
    person->state = PERSON_NOT_ARRIVED;
}

// Проверка доставки всех пассажиров
bool allPeopleDelivered(Person *personArray, int totalPeople) {
    for (int i = 0; i < totalPeople; i++) {
        if (personArray[i].state != PERSON_DELIVERED) {
            return false;
        }
    }

    return true;
}
