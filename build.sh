#!/bin/bash

if [[ "$DEBUG" == "1" ]]; then
  DEBUG_FLAG="-DDEBUG=1"
else
  DEBUG_FLAG=""
fi

bear -- clang \
  -Wall -Werror -g \
  $DEBUG_FLAG \
  -o build/kvxdb \
  src/main.c
