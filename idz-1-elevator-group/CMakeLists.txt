cmake_minimum_required(VERSION 3.16)
project(elevator_group C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(elevator
    src/actions.c
    src/main.c
    src/dispatcher.c
    src/input.c
    src/interruption.c
    src/lift.c
    src/logger.c
    src/options.c
    src/person.c
    src/route.c
    src/service.c
    src/simulation.c
    src/statistics.c
)

target_compile_options(elevator PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)

if(BUILD_TESTING)
    function(add_elevator_test test_name config_file)
        add_test(
            NAME ${test_name}
            COMMAND elevator
                --config ${CMAKE_CURRENT_SOURCE_DIR}/data/${config_file}
                --log ${CMAKE_CURRENT_BINARY_DIR}/${test_name}.log
        )
        set_tests_properties(
            ${test_name}
            PROPERTIES
                PASS_REGULAR_EXPRESSION
                "Simulation finished. All people delivered."
        )
    endfunction()

    add_elevator_test(simple simple.txt)
    add_elevator_test(capacity_one capacity-one.txt)
    add_elevator_test(opposite_directions opposite-directions.txt)
    add_elevator_test(same_floor same-floor.txt)
    add_elevator_test(different_initial_floors different-initial-floors.txt)
    add_elevator_test(many_passengers many-passengers.txt)

    add_test(
        NAME work_period
        COMMAND elevator
            --config ${CMAKE_CURRENT_SOURCE_DIR}/data/work-period.txt
            --log ${CMAKE_CURRENT_BINARY_DIR}/work_period.log
    )
    set_tests_properties(
        work_period
        PROPERTIES PASS_REGULAR_EXPRESSION "Configured work period finished."
    )
endif()
