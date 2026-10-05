#!/bin/sh
# Measure the per-frame DSP cost under simavr.
#
# Needs the PlatformIO AVR toolchain and the simavr tool:
#     pio pkg install -g -t tool-simavr
#
# Everything it reports is measured, not estimated - if you change the FFT or
# the feature extractor, rerun this before you claim the frame budget still
# holds.
set -e
cd "$(dirname "$0")/.."
PIO_PKG="${PIO_PKG:-$HOME/.platformio/packages}"
AVR="$PIO_PKG/toolchain-atmelavr/bin"
SIMAVR_INC="$PIO_PKG/tool-simavr/include/simavr"
OUT="${TMPDIR:-/tmp}/vpl_bench.elf"

"$AVR/avr-g++" -mmcu=atmega328p -DF_CPU=16000000UL -Os -std=gnu++17 \
    -fno-exceptions -fno-rtti -Iinclude -I"$SIMAVR_INC" \
    -c test/bench_frame.cpp -o "${OUT%.elf}.o"
"$AVR/avr-gcc" -mmcu=atmega328p -DF_CPU=16000000UL -Os -std=gnu99 \
    -I"$SIMAVR_INC" -c test/bench_mmcu.c -o "${OUT%.elf}_mmcu.o"
"$AVR/avr-g++" -mmcu=atmega328p -Os -fno-exceptions -fno-rtti -Iinclude \
    "${OUT%.elf}.o" "${OUT%.elf}_mmcu.o" \
    src/fft64.cpp src/feature_extract.cpp src/recognizer.cpp \
    -o "$OUT" -lm
exec "$PIO_PKG/tool-simavr/bin/simavr" -m atmega328p -f 16000000 "$OUT"
