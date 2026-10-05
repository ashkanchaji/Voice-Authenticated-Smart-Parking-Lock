# Proteus simulation

## Building the circuit is part of the assignment

No `.pdsprj` is included. You draw the schematic yourself, and the finished
project file is one of the deliverables (see [../ASSIGNMENT.md](../ASSIGNMENT.md)).

What is here is a complete build specification, detailed enough that you should
not have to guess at any connection:

| File | Contents |
|---|---|
| [BOM.csv](BOM.csv) | every component, with its exact Proteus device name |
| [wiring.md](wiring.md) | every net, connection by connection |
| [pin-map.md](pin-map.md) | the pin assignment and the reasoning behind it |
| [validation-checklist.md](validation-checklist.md) | what to measure, and the expected value |

If following [wiring.md](wiring.md) does require guessing anywhere, that is a
defect in the document — say so and it will be fixed.

## Building the circuit

1. New project → **Schematic Capture** only (no PCB needed).
2. Place the parts from [BOM.csv](BOM.csv). The `Proteus device` column is the
   exact name to type into the Pick Devices dialog.
3. Wire it up following [wiring.md](wiring.md), section by section.
4. Double-click the Arduino Uno and set:
   * **Program File** → `.pio/build/uno/firmware.hex`
   * **Clock Frequency** → `16 MHz`
5. Add the virtual terminal (115200 8N1) and the oscilloscope as described in
   section 8 of the wiring guide.

## Producing the firmware

```sh
cd student-starter
pio run
# -> .pio/build/uno/firmware.hex   (load this into Proteus)
# -> .pio/build/uno/firmware.elf   (load this instead for source-level debug)
```

## Producing the audio stimulus

Proteus drives A0 from a single audio source, so the whole session — three
training utterances and the recognition tests — has to be one continuous WAV
with silence in between:

```sh
python3 tools/prepare_voice_dataset.py \
    --train assets/train1.wav assets/train2.wav assets/train3.wav \
    --test  assets/correct.wav assets/wrong_phrase.wav assets/wrong_speaker.wav \
    -o assets/scenario.wav
```

The tool prints a timeline showing when each utterance starts. Keep it — you
need it to know when to press the training button during the simulation.

See [../assets/README.md](../assets/README.md) for how to record the source
files. **No recordings are included** — you supply them, and how you record them
matters more than you would expect. Read that document before you press record.

## Running a session

1. Start the simulation. Once your firmware works, the virtual terminal should
   show:

   ```
   BOOT: VOICE PARKING LOCK
   SAMPLE RATE HZ = 8000
   ...
   EEPROM INVALID
   TRAINING REQUIRED
   TRAINING: PRESS BUTTON, THEN SAY THE PASSPHRASE (1/3)
   ```

2. Press the training button just before each training utterance starts (use
   the timeline the dataset tool printed). After the third one you should see
   `TEMPLATE SAVED` and a `THRESHOLD = ...` line.
3. Let the test utterances play. Each produces a `MATCH SCORE` and either
   `ACCESS GRANTED` or `ACCESS DENIED`.
4. On a grant, watch the servo swing, the green LED light, and the DAC
   staircase appear on the scope — all at once.

Proteus does not preserve EEPROM between runs by default, so the device asks
for training on every fresh start. To test persistence, enable the Arduino
model's EEPROM file property, or simply reset the MCU mid-simulation and watch
it report `EEPROM VALID`.

## What to look at

Work through [validation-checklist.md](validation-checklist.md). The three
measurements that matter most:

* **ADC** — the biased audio at A0, never touching either rail;
* **PWM** — 50 Hz on D9, the pulse widening from 1 ms to 2 ms on a grant;
* **DAC** — a 64-step staircase at the ladder output, and the same waveform
  smoothed after the reconstruction filter.
