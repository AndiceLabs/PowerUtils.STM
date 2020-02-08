#!/bin/bash

grep -q AM33XX /proc/cpuinfo

if [ $? -eq 0 ]; then
    echo TRUE
fi

