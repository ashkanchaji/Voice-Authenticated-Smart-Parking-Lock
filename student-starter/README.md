# Student starter — voice-authenticated parking lock

Build a voice-operated parking gate on an Arduino Uno R3 (ATmega328P) and
simulate it in Proteus. The full brief is in [ASSIGNMENT.md](ASSIGNMENT.md);
the marks are in [GRADING_RUBRIC.md](GRADING_RUBRIC.md).

## What you have been given

* the complete project structure, build configuration and pin map;
* every module's header, documenting exactly what each function must do;
* the fixed-point FFT, the Hamming window and the twiddle tables — finished;
* `success_audio.h`, a 6-bit PCM clip ready to play;
* a working debug UART;
* the state enum and the main dispatch loop;
* a host test suite that is the specification in executable form;
* a cycle benchmark that measures your per-frame DSP cost under simavr;
* the full Proteus wiring specification;
* the Python tooling for preparing audio.

## What you have to write

Everything marked `TODO` — ADC configuration and sampling, the timer and
interrupt architecture, voice activity detection, feature extraction, training,
template matching, threshold calculation, EEPROM storage, PWM setup, servo
control, DAC playback, and the state machine behaviour.

Search for it:

```sh
grep -rn "TODO" src/ include/
```

## First five minutes

```sh
pio run          # it builds. It does nothing useful, but it builds.
./test/run.sh    # it fails. That is the specification telling you what is missing.
```

Keep both of those working. A starter that no longer compiles is much harder to
get marks for than one that compiles and does half the job.

## Suggested order

1. **Timer0 and the UART.** Nothing else can be debugged until `systick_ms()`
   advances and the terminal prints. Prove it with the button: a short press
   should be distinguishable from a two-second hold.
2. **Timer1 and the servo.** Pure output, no dependencies, and you get an
   oscilloscope trace you can measure against a known number the same afternoon.
3. **Timer2 and the DAC.** Also pure output. The provided audio clip starts
   with a 0→63 staircase precisely so a single scope trace tells you whether the
   ladder, the port masking and the interrupt all work.
4. **Timer2 and the ADC.** Now the input side, with the ping-pong buffer. Check
   the biased signal at A0 before you trust anything downstream.
5. **The DSP, on your laptop.** `./test/run.sh` runs the real feature extractor
   on the host. Debug the scaling there, where a failing case reproduces in
   milliseconds, not in a simulator.
6. **VAD, then training, then matching, then EEPROM.**
7. **The state machine**, tying it together.

Do not start at step 7.

## Rules

Banned in the main implementation, in both directions:

```
analogRead()   analogWrite()   Servo.h   Servo.write()   tone()   delay()
```

ADC, PWM, timers, DAC and EEPROM must be register-level. UART debug output is
allowed and encouraged. See [ASSIGNMENT.md](ASSIGNMENT.md) for the full list and
the reasons — each ban exists because the library version would break something
specific in this design, not for the sake of it.

## Documentation

| Document | What it covers |
|---|---|
| [ASSIGNMENT.md](ASSIGNMENT.md) | the brief: goals, deliverables, demonstration |
| [GRADING_RUBRIC.md](GRADING_RUBRIC.md) | how the 100 marks are allocated |
| [docs/architecture.md](docs/architecture.md) | signal path, timer ownership, state machine |
| [docs/recognition-algorithm.md](docs/recognition-algorithm.md) | the DSP pipeline and its limitations |
| [docs/adc.md](docs/adc.md) | ADC requirements and the input circuit |
| [docs/pwm-servo.md](docs/pwm-servo.md) | Timer1 and servo calibration |
| [docs/r2r-dac.md](docs/r2r-dac.md) | the ladder, the masking, the output stage |
| [docs/timers.md](docs/timers.md) | timer allocation and the frame budget |
| [docs/memory-budget.md](docs/memory-budget.md) | a worksheet for your own numbers |
| [docs/proteus-testing.md](docs/proteus-testing.md) | what to measure and why |
| [docs/cornell-comparison.md](docs/cornell-comparison.md) | where the idea came from |
| [proteus/wiring.md](proteus/wiring.md) | every net, connection by connection |
| [assets/README.md](assets/README.md) | how to record the audio you need |
