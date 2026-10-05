# Audio assets

**This directory ships empty of audio on purpose.** No recordings are included.

Generating a WAV file and labelling it "the authorised speaker saying «پارکینگ
باز شو»" would be a fabrication: there is no such speaker and no such
recording. Every audio file this project needs has to come from a real person
at your end. What is here instead is the exact specification of what to record
and the tools to convert it.

---

## 1. The passphrase recordings

Six files, all of the same Persian passphrase except where noted.

| File | Speaker | Phrase | Purpose |
|---|---|---|---|
| `train1.wav` | the authorised person | «پارکینگ باز شو» | training utterance 1 |
| `train2.wav` | the authorised person | «پارکینگ باز شو» | training utterance 2 |
| `train3.wav` | the authorised person | «پارکینگ باز شو» | training utterance 3 |
| `correct.wav` | the authorised person | «پارکینگ باز شو» | case 1 — expect ACCEPT |
| `wrong_phrase.wav` | the authorised person | any other phrase | case 2 — expect REJECT |
| `wrong_speaker.wav` | a different person | «پارکینگ باز شو» | case 3 — expect REJECT |

Optionally add `wrong_both.wav` (different person, different phrase) for case 4.

### Recording requirements

| Property | Requirement | Why |
|---|---|---|
| Duration of speech | **1.2 – 1.6 s** | the capture window is exactly 1.6 s; longer is truncated |
| Leading silence | 0.2 – 0.5 s | gives the VAD's noise-floor estimator something to settle on |
| Sample rate | anything ≥ 8 kHz | the tools resample; 16 kHz or 44.1 kHz is fine |
| Channels | mono preferred | the tools downmix stereo |
| Format | uncompressed PCM WAV | 8, 16, 24 or 32-bit integer |
| Level | peaks around −6 dBFS, no clipping | clipping adds harmonics the recogniser will learn |
| Background | quiet, same room for all six | the VAD adapts, but the *features* do not |

**Record all three training utterances in one sitting**, in the same room, at
the same distance from the microphone, at the same pace. The acceptance
threshold is derived from how much those three disagree with each other: if
they were recorded in three different acoustic conditions, the threshold comes
out loose and the lock stops discriminating. If they were recorded to be
artificially identical (by copying one file three times), the threshold comes
out at the floor and the speaker fails on their next real attempt.

Say the phrase at the same speed each time. The system has no time alignment —
segments are fixed 200 ms windows — so a 15% speed change moves every segment
boundary. See the limitations in
[../docs/recognition-algorithm.md](../docs/recognition-algorithm.md#limitations).

### Building the Proteus stimulus

```sh
python3 tools/prepare_voice_dataset.py \
    --train assets/train1.wav assets/train2.wav assets/train3.wav \
    --test  assets/correct.wav assets/wrong_phrase.wav assets/wrong_speaker.wav \
    -o assets/scenario.wav
```

This resamples to 8 kHz, downmixes to mono, removes DC, normalises every clip
to the same peak, inserts a 1 s lead-in and 5 s gaps, and prints a timeline:

```
  start      end   kind   clip
  -----   ------   ----   ----
   1.00     2.45   train  train1.wav
   7.45     8.88   train  train2.wav
  ...
```

Keep the timeline. It tells you when to press the training button during the
simulation.

The equal-peak normalisation is deliberate. If the authorised speaker's clip
were simply louder than the impostor's, the test would prove nothing about
identity.

---

## 2. The success message — «ورود مجاز است»

`include/success_audio.h` currently contains a **placeholder test tone**, not
the Persian message. No Persian text-to-speech engine was available when this
repository was generated, and generating some other audio and labelling it as
this phrase would be dishonest about what the firmware plays. The header says
so in a comment, the firmware announces it over UART at boot
(`AUDIO: PLACEHOLDER TEST TONE, NOT THE PERSIAN MESSAGE`), and
`SUCCESS_AUDIO_IS_PLACEHOLDER` is set to 1 so code can test for it.

The placeholder is not filler: it is a 0 → 63 staircase followed by a 1 kHz
sine, chosen so that a single oscilloscope trace proves the ladder, the PORTD
masking and the Timer2 playback ISR all work. See
[../docs/r2r-dac.md](../docs/r2r-dac.md).

### Shipping the real message

1. Record or synthesise someone saying «ورود مجاز است» and save it as
   `assets/success_fa.wav`. Keep it **under 1.8 seconds** — one byte of flash
   per sample at 8 kHz means 8000 bytes per second, and the firmware needs
   about 9.4 KB of the 31.5 KB available.
2. Convert it:

   ```sh
   python3 tools/wav_to_6bit_header.py assets/success_fa.wav \
       -o include/success_audio.h
   ```

3. Rebuild (`pio run`) and check the reported flash usage.

The tool resamples to 8 kHz, low-passes before decimating, removes DC,
normalises, quantises to 6 bits and emits a `PROGMEM` array. It refuses to
generate more than `--max-seconds` (default 1.8 s) so an oversized recording
fails with a clear message instead of an obscure link error.

Without `--placeholder`, the generated header sets
`SUCCESS_AUDIO_IS_PLACEHOLDER 0` and the boot warning disappears by itself.

### Regenerating the placeholder

```sh
python3 tools/make_test_tone.py -o /tmp/test_tone.wav
python3 tools/wav_to_6bit_header.py /tmp/test_tone.wav \
    -o include/success_audio.h --placeholder --no-normalize
```

---

## 3. Checking a recording before you trust it

```sh
python3 - <<'PY'
import sys; sys.path.insert(0, "tools")
import wavkit
s, rate = wavkit.read_wav("assets/train1.wav")
print(f"{len(s)/rate:.2f} s at {rate} Hz, peak {max(map(abs, s)):.3f}")
PY
```

* Duration between about 1.4 s and 2.1 s (0.2–0.5 s of lead-in plus 1.2–1.6 s
  of speech).
* Peak below 1.0. A peak of exactly 1.0 usually means the recording clipped.

---

## `.gitignore` note

`scenario.wav` is generated and can be rebuilt from the sources at any time;
there is no need to commit it. The six source recordings are the only files
worth keeping under version control, and whether that is appropriate depends on
whether the people recorded consented to it.
