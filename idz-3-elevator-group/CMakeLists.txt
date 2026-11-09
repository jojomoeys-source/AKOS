cmake_minimum_required(VERSION 3.16)
project(elevator_group C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

set(ELEVATOR_CORE_SOURCES
    src/actions.c
    src/call_queue.c
    src/context.c
    src/dispatcher.c
    src/dispatcher_process.c
    src/dispatcher_thread.c
    src/input.c
    src/interruption.c
    src/ipc_sync.c
    src/lift.c
    src/lift_process.c
    src/lift_thread.c
    src/logger.c
    src/options.c
    src/person.c
    src/person_process.c
    src/person_thread.c
    src/process_simulation.c
    src/route.c
    src/service.c
    src/shared_state.c
    src/shared_queue.c
    src/simulation.c
    src/statistics.c
)

function(add_elevator_program target entry_point)
    add_executable(${target} ${entry_point} ${ARGN} ${ELEVATOR_CORE_SOURCES})
    target_link_libraries(${target} PRIVATE Threads::Threads)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endfunction()

add_elevator_program(elevator src/main.c)
add_elevator_program(elevator_setup src/independent_setup_main.c src/independent_worker.c)
add_elevator_program(elevator_dispatcher src/independent_dispatcher_main.c src/independent_worker.c)
add_elevator_program(elevator_person src/independent_person_main.c src/independent_worker.c)
add_elevator_program(elevator_lift src/independent_lift_main.c src/independent_worker.c)
add_elevator_program(elevator_run src/independent_run_main.c)
add_elevator_program(elevator_cleanup src/independent_cleanup_main.c)

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
                TIMEOUT 15
        )
    endfunction()

    add_elevator_test(simple simple.txt)
    add_elevator_test(capacity_one capacity-one.txt)
    add_elevator_test(opposite_directions opposite-directions.txt)
    add_elevator_test(same_floor same-floor.txt)
    add_elevator_test(different_initial_floors different-initial-floors.txt)
    add_elevator_test(many_passengers many-passengers.txt)
    add_elevator_test(simultaneous_calls simultaneous-calls.txt)
    add_elevator_test(dispatcher_retry dispatcher-retry.txt)

    add_test(
        NAME work-period
        COMMAND elevator
            --config ${CMAKE_CURRENT_SOURCE_DIR}/data/work-period.txt
            --log ${CMAKE_CURRENT_BINARY_DIR}/work_period.log
    )
    set_tests_properties(
        work-period
        PROPERTIES
            PASS_REGULAR_EXPRESSION
                "Configured work period finished."
            TIMEOUT 15
    )

endif()
