#!/bin/bash
cmake -DCMAKE_BUILD_TYPE=RELWITHDEBINFO .
make cleanDesigns
make clean
