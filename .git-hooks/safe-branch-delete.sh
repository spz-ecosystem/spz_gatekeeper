#!/bin/bash
# Safe branch delete: prevent deleting main/master
for arg in "$@"; do
  if [ "$arg" = "main" ] || [ "$arg" = "master" ]; then
    echo "ERROR: Cannot delete main/master branch!"
    echo "If you really need to, use: git branch -D $arg (uppercase D)"
    exit 1
  fi
done
git branch -d "$@"
