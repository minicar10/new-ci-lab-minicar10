#!/usr/bin/env bash

# Directories
TEST_DIR="testcases/week2"
INPUT_DIR="inputs"
OUTPUT_DIR="outputs"
TMP_DIR="tmp_outputs"  # Temporary directory for intermediate outputs

# Create necessary directories
mkdir -p "${INPUT_DIR}"
mkdir -p "${OUTPUT_DIR}"
mkdir -p "${TMP_DIR}"

# Loop through all `.s` files in the test directory
for TEST_FILE in ${TEST_DIR}/*.s; do
    TEST_NAME=$(basename "${TEST_FILE}" .s)

    echo "Testing: ${TEST_NAME}"

    # Save input file copy
    cp "${TEST_FILE}" "${INPUT_DIR}/${TEST_NAME}.s"

    # Run the program and save output in a temporary file
    bin/ci -i "${TEST_FILE}" > "${TMP_DIR}/${TEST_NAME}_my_output.txt"

    # Run the reference program and save output in a temporary file
    bin/ci_reference -i "${TEST_FILE}" > "${TMP_DIR}/${TEST_NAME}_ref_output.txt"

    # Compare outputs and decide whether to keep them
    if diff -u "${TMP_DIR}/${TEST_NAME}_my_output.txt" "${TMP_DIR}/${TEST_NAME}_ref_output.txt" > "${TMP_DIR}/${TEST_NAME}_diff.txt"; then
        echo "✅ Passed: ${TEST_NAME}"
        # Remove temporary files since the test passed
        rm -f "${TMP_DIR}/${TEST_NAME}_my_output.txt"
        rm -f "${TMP_DIR}/${TEST_NAME}_ref_output.txt"
        rm -f "${TMP_DIR}/${TEST_NAME}_diff.txt"
    else
        echo "❌ Failed: ${TEST_NAME} - See ${OUTPUT_DIR}/${TEST_NAME}_diff.txt"
        # Move only failing test outputs to the final outputs directory
        mv "${TMP_DIR}/${TEST_NAME}_my_output.txt" "${OUTPUT_DIR}/"
        mv "${TMP_DIR}/${TEST_NAME}_ref_output.txt" "${OUTPUT_DIR}/"
        mv "${TMP_DIR}/${TEST_NAME}_diff.txt" "${OUTPUT_DIR}/"
    fi
done

# Cleanup: Remove the temporary directory (only if empty)
rmdir --ignore-fail-on-non-empty "${TMP_DIR}"
