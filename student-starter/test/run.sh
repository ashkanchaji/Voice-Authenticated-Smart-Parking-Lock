#!/bin/sh
# Build and run the host-side tests. No framework, no fixtures - one binary
# that returns non-zero if any assertion fails.
#
# These fail against the starter stubs. That is the point: they are the
# specification, and making them pass is the fastest route through the DSP half
# of the assignment.
set -e
cd "$(dirname "$0")/.."
g++ -std=c++17 -O2 -Wall -Wextra -Iinclude \
    test/host_tests.cpp src/recognizer.cpp src/fft64.cpp src/feature_extract.cpp src/vad.cpp \
    -o /tmp/vpl_student_tests -lm
exec /tmp/vpl_student_tests
