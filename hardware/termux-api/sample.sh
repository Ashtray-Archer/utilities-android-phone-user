#!/bin/sh
set -eu

if ! command -v termux-sensor >/dev/null 2>&1; then
    echo "termux-sensor is unavailable; install the Termux:API app and package first" >&2
    exit 2
fi

termux-sensor -s Accelerometer -d 20 -n 1 |
awk '
    /"values"[[:space:]]*:/ {
        reading_values = 1
        next
    }

    reading_values && value_count < 3 {
        value = $0
        gsub(/[[:space:],]/, "", value)
        if (value ~ /^[-+]?[0-9]+([.][0-9]+)?([eE][-+]?[0-9]+)?$/) {
            values[value_count] = value
            value_count += 1
        }
    }

    value_count == 3 {
        print values[0], values[1], values[2]
        found = 1
        exit
    }

    END {
        if (!found) {
            print "could not find one three-axis sensor value in Termux:API output" > "/dev/stderr"
            exit 1
        }
    }
'
