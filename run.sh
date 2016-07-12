#!/usr/bin/env bash

gcc -g -O0 -std=c99 -Wall -Wfloat-equal -Wtype-limits -Wpointer-arith -Wlogical-op src/venmoGraphParams.h src/list.h src/list.c src/table.h src/table.c src/main.c -o venGraph

./venGraph venmo_input/venmo-trans.txt venmo_output/output.txt
