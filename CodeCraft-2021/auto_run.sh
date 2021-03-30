#!/bin/bash

g++ -D LOCAL_DEBUG CodeCraft-2021.cpp -o CodeCraft-2021
go build -o debug debug.go
./debug
