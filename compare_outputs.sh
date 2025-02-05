#!/usr/bin/env bash

TEST_DIR="testcases/week4"
INPUT_DIR="inputs"
OUTPUT_DIR="outputs"
TMP_DIR="tmp_outputs"  
FAILED_DIR="failed_details" 

mkdir -p "${INPUT_DIR}"
mkdir -p "${OUTPUT_DIR}"
mkdir -p "${TMP_DIR}"
mkdir -p "${FAILED_DIR}"

for TEST_FILE in ${TEST_DIR}/*.s; do
    TEST_NAME=$(basename "${TEST_FILE}" .s)

    cp "${TEST_FILE}" "${INPUT_DIR}/${TEST_NAME}.s"

    bin/ci -i "${TEST_FILE}" > "${TMP_DIR}/${TEST_NAME}_my_output.txt"

    bin/ci_reference -i "${TEST_FILE}" > "${TMP_DIR}/${TEST_NAME}_ref_output.txt"

    if diff -u "${TMP_DIR}/${TEST_NAME}_my_output.txt" "${TMP_DIR}/${TEST_NAME}_ref_output.txt" > "${TMP_DIR}/${TEST_NAME}_diff.txt"; then
        echo "✅ Passed: ${TEST_NAME}"
        rm -f "${TMP_DIR}/${TEST_NAME}_my_output.txt"
        rm -f "${TMP_DIR}/${TEST_NAME}_ref_output.txt"
        rm -f "${TMP_DIR}/${TEST_NAME}_diff.txt"
    else
        echo "❌ Failed: ${TEST_NAME} - See ${FAILED_DIR}/${TEST_NAME}_report.txt"

        mv "${TMP_DIR}/${TEST_NAME}_my_output.txt" "${OUTPUT_DIR}/"
        mv "${TMP_DIR}/${TEST_NAME}_ref_output.txt" "${OUTPUT_DIR}/"
        mv "${TMP_DIR}/${TEST_NAME}_diff.txt" "${OUTPUT_DIR}/"

        {
            echo "Actual input:"
            cat "${TEST_FILE}"
            echo
            echo "my output:"
            cat "${OUTPUT_DIR}/${TEST_NAME}_my_output.txt"
            echo
            echo "ref output:"
            cat "${OUTPUT_DIR}/${TEST_NAME}_ref_output.txt"
        } > "${FAILED_DIR}/${TEST_NAME}_report.txt"
    fi
done

rmdir --ignore-fail-on-non-empty "${TMP_DIR}"
