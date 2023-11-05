#!/bin/bash
set -xeu
g++ -Wall -Wextra -pedantic -g src/main.cpp $@
