#!/bin/env zsh

REPO_ROOT=$(git rev-parse --show-toplevel)
cd "${REPO_ROOT}" || exit

ruff format .
ruff check --fix .
fdfind -e c -e cpp -e h -e hpp -x clang-format -i
fdfind -e zsh -x shellcheck -s bash
fdfind -e sh -x shellcheck -s sh
fdfind -e bash -x shellcheck -s bash
