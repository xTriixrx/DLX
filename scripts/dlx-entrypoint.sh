#!/bin/sh
set -eu

if [ "$#" -eq 0 ]; then
  exec /usr/local/bin/dlx --server "${DLX_PROBLEM_PORT:-7000}" "${DLX_SOLUTION_PORT:-7001}"
fi

exec /usr/local/bin/dlx "$@"
