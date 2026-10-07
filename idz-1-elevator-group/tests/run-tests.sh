#!/usr/bin/env bash

set -u

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$project_dir/build"
data_dir="$project_dir/data"
results_dir="$project_dir/tests/results"

tests=(
    simple
    capacity-one
    opposite-directions
    same-floor
    different-initial-floors
    many-passengers
    work-period
)

mkdir -p "$results_dir"

echo "Building project..."
if ! cmake -S "$project_dir" -B "$build_dir"; then
    echo "Build configuration failed."
    exit 1
fi

if ! cmake --build "$build_dir" --clean-first; then
    echo "Build failed."
    exit 1
fi

passed=0
failed=0
failed_tests=()

echo
echo "Running tests..."

for test_name in "${tests[@]}"; do
    input_file="$data_dir/$test_name.txt"
    output_file="$results_dir/$test_name.log"

    expected="Simulation finished. All people delivered."
    if [[ "$test_name" == "work-period" ]]; then
        expected="Configured work period finished."
    fi

    if "$build_dir/elevator" --config "$input_file" \
           --log "$results_dir/$test_name-events.log" \
           > "$output_file" 2>&1 && grep -q "$expected" "$output_file"; then
        echo "[PASS] $test_name"
        passed=$((passed + 1))
    else
        echo "[FAIL] $test_name"
        failed=$((failed + 1))
        failed_tests+=("$test_name")
    fi
done

total=${#tests[@]}

echo
echo "Passed: $passed of $total"
echo "Failed: $failed of $total"

if (( failed > 0 )); then
    echo "Failed tests: ${failed_tests[*]}"
    echo "Detailed output: $results_dir"
    exit 1
fi

echo "All tests passed."
