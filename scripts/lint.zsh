#!/usr/bin/env zsh

# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: Copyright The Kabot Project Contributors


REPO_ROOT=$(git rev-parse --show-toplevel)
cd "${REPO_ROOT}" || exit

# shellcheck source=/dev/null
source .venv/bin/activate

# Check for required tools
for tool in clang-format shellcheck ruff fdfind yamllint; do
    if ! command -v "$tool" &> /dev/null; then
        echo "Error: $tool is not installed" >&2
        exit 1
    fi
done

# Use arrays for flags so zsh splits the arguments correctly
RUFF_CHECK_FLAGS=()
RUFF_FORMAT_FLAGS=(--check)
CLANG_FLAGS=(--dry-run -Werror)

if [[ "$1" == "--fix" ]]; then
    # Switch to "fix" mode
    RUFF_CHECK_FLAGS=(--fix)
    RUFF_FORMAT_FLAGS=()
    CLANG_FLAGS=(-i)
fi

echo "Running linting and formatting checks..."
echo "-------------------------------------------------------------------"

# Initialize our failure tracker
FAILED=0

echo "Running ruff format..."
ruff format "${RUFF_FORMAT_FLAGS[@]}" . || FAILED=1

echo "Running ruff check..."
ruff check "${RUFF_CHECK_FLAGS[@]}" . || FAILED=1

echo "Running Zephyr compliance check..."
./deps/zephyr/scripts/ci/check_compliance.py -e Gitlint -e SysbuildKconfig -e SysbuildKconfigBasic -e SysbuildKconfigBasicNoModules -e Identity -e Kconfig -e KconfigBasic -e KconfigBasicNoModules -e KconfigHWMv2 || FAILED=1


echo "Running clang-format..."
fdfind -e c -e cpp -e h -e hpp -x clang-format "${CLANG_FLAGS[@]}" || FAILED=1

echo "Running shellcheck..."
fdfind -e zsh -x shellcheck -s bash || FAILED=1
fdfind -e sh -x shellcheck -s sh || FAILED=1
fdfind -e bash -x shellcheck -s bash || FAILED=1

echo "Running yamllint..."
fdfind -e yaml -e yml -X yamllint || FAILED=1

echo "-------------------------------------------------------------------"

# Evaluate the tracker
if [[ $FAILED -ne 0 ]]; then
    echo "ERROR: Formatting or linting issues were detected." >&2
    exit 1
else
    echo "SUCCESS: All checks passed!"
    exit 0
fi
