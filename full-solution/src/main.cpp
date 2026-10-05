/* Voice-Authenticated Smart Parking Lock - reference implementation.
 *
 * Arduino Uno R3 / ATmega328P, built without the Arduino framework so that the
 * firmware owns all three timers outright.
 *
 *   Timer0  1 ms system tick          systick.cpp
 *   Timer1  50 Hz servo PWM (OC1A)    servo.cpp
 *   Timer2  8 kHz ADC / DAC clock     timer2.cpp
 *
 * The main loop never blocks. Everything with a duration - the 1.6 s capture,
 * the 3 s gate-open, the 1 s reject indication, the audio clip - is a deadline
 * compared against systick_ms(), not a delay.
 */

#include "config.h"
#include "pins.h"
#include "static_checks.h"
#include "systick.h"
#include "uart.h"
#include "timer2.h"
#include "adc_sampler.h"
#include "vad.h"
#include "feature_extract.h"
#include "recognizer.h"
#include "template_store.h"
#include "servo.h"
#include "dac_audio.h"
#include "success_audio.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>

/** Give up on an utterance if nobody speaks, in milliseconds. */
static const uint32_t LISTEN_TIMEOUT_MS = 15000ul;

enum class SystemState : uint8_t {
    BOOT,
    TRAINING_WAIT,   /**< waiting for the button before recording an utterance */
    TRAINING_LISTEN, /**< recording one of the TRAINING_UTTERANCES */
    IDLE,            /**< armed, tracking the noise floor, waiting for speech */
    LISTENING,       /**< capturing the 1.6 s test utterance */
    PROCESSING,      /**< scoring the captured utterance */
    ACCEPTED,        /**< match: open the gate and start the message */
    REJECTED,        /**< no match: show the red LED */
    GATE_OPEN,       /**< gate held open, audio streaming, waiting out 3 s */
};

static SystemState g_state = SystemState::BOOT;
static uint32_t g_state_since;

/** Normalised features of the utterance just captured. */
static uint8_t g_feature[FEATURE_COUNT];
/** The first TRAINING_UTTERANCES-1 training vectors; the last one is g_feature. */
static uint8_t g_train[TRAINING_UTTERANCES - 1u][FEATURE_COUNT];
static uint8_t g_train_count;

/** Active template plus its threshold, mirroring the EEPROM record. */
static StoredTemplate g_template;

/** True while frames are being folded into the feature accumulators. */
static bool g_capturing;
/** Set by the frame pump once CAPTURE_FRAMES frames have been consumed. */
static bool g_capture_done;
/** Latches so "AUDIO END" is printed once, not once per loop iteration. */
static bool g_audio_end_reported;

/* ------------------------------------------------------------------ LEDs */

static void led_init(void)
{
    LED_DDR |= _BV(LED_GREEN_BIT) | _BV(LED_RED_BIT);
    LED_PORT &= (uint8_t)~(_BV(LED_GREEN_BIT) | _BV(LED_RED_BIT));
}

static void leds_set(bool green, bool red)
{
    if (green) {
        LED_PORT |= _BV(LED_GREEN_BIT);
    } else {
        LED_PORT &= (uint8_t)~_BV(LED_GREEN_BIT);
    }
    if (red) {
        LED_PORT |= _BV(LED_RED_BIT);
    } else {
        LED_PORT &= (uint8_t)~_BV(LED_RED_BIT);
    }
}

/* ---------------------------------------------------------------- button */

struct ButtonEvent {
    bool short_press;
    bool long_press;
};

static void button_init(void)
{
    BUTTON_DDR &= (uint8_t)~_BV(BUTTON_BIT);
    BUTTON_PORT |= _BV(BUTTON_BIT); /* internal pull-up: idle high, pressed low */
}

/** Debounced edge detector with a long-press branch.
 *
 * The long press fires while the button is still held rather than on release,
 * so the operator gets feedback at the two-second mark instead of wondering
 * whether they held it long enough.
 */
static ButtonEvent button_poll(void)
{
    static bool s_down;
    static bool s_long_fired;
    static uint32_t s_edge_ms;
    ButtonEvent ev = { false, false };

    const bool raw = (BUTTON_PIN & _BV(BUTTON_BIT)) == 0u;
    const uint32_t now = systick_ms();

    if (raw != s_down) {
        if (now - s_edge_ms < BUTTON_DEBOUNCE_MS) {
            return ev; /* still inside the bounce window */
        }
        s_edge_ms = now;
        s_down = raw;
        if (raw) {
            s_long_fired = false;
        } else if (!s_long_fired) {
            ev.short_press = true;
        }
        return ev;
    }

    if (s_down && !s_long_fired && (now - s_edge_ms) >= BUTTON_LONG_PRESS_MS) {
        s_long_fired = true;
        ev.long_press = true;
    }
    return ev;
}

/* ------------------------------------------------------------ frame pump */

/** Consume any frame the ADC interrupt has finished with.
 *
 * While idle it runs the VAD; once speech is confirmed it switches to folding
 * frames into the feature accumulators. The frame that triggered the START
 * edge is itself captured, so only the VAD_START_FRAMES-1 frames before it are
 * lost - about 16 ms of the leading consonant.
 */
static void pump_frames(void)
{
    if (!adc_frame_ready()) {
        return;
    }
    const int16_t *frame = adc_frame_get();

    if (g_capturing) {
        if (feature_process_frame(frame)) {
            g_capturing = false;
            g_capture_done = true;
        }
    } else {
        const uint16_t mag = feature_frame_magnitude(frame);
        if (vad_process_frame(mag) == VadEvent::START) {
            feature_reset();
            g_capturing = true;
            /* 14 bytes at 115200 baud is 1.2 ms; with 1.75 ms of DSP that is
             * 3 ms of the 8 ms frame. It is the only print inside the capture
             * path, and it has to stay that way. */
            UART_LINE("SPEECH START");
            (void)feature_process_frame(frame);
        }
    }
    adc_frame_release();
}

/* ------------------------------------------------------------ transitions */

static void enter(SystemState s)
{
    g_state = s;
    g_state_since = systick_ms();
}

static void start_listening(void)
{
    g_capturing = false;
    g_capture_done = false;
    feature_reset();
    vad_reset();
    adc_overrun_reset();
    adc_sampling_start();
}

static void report_capture_quality(void)
{
    const uint16_t drops = adc_overrun_count();
    if (drops != 0u) {
        /* Never expected: the FFT takes about 2.5 ms of the 8 ms frame budget.
         * If this is ever non-zero the timing analysis in docs/timers.md is
         * wrong and the features are missing samples, so it must be visible. */
        uart_kv(PSTR("ADC OVERRUN"), drops);
    }
}

static void enter_training_wait(void)
{
    adc_sampling_stop();
    leds_set(false, false);
    uart_puts_P(PSTR("TRAINING: PRESS BUTTON, THEN SAY THE PASSPHRASE ("));
    uart_put_u32(g_train_count + 1u);
    uart_puts_P(PSTR("/"));
    uart_put_u32(TRAINING_UTTERANCES);
    UART_LINE(")");
    enter(SystemState::TRAINING_WAIT);
}

static void enter_idle(void)
{
    leds_set(false, false);
    start_listening();
    UART_LINE("IDLE - LISTENING FOR PASSPHRASE");
    enter(SystemState::IDLE);
}

static void erase_template_and_retrain(void)
{
    audio_stop();
    adc_sampling_stop();
    servo_lock();
    template_clear();
    memset(&g_template, 0, sizeof(g_template));
    g_train_count = 0;
    UART_LINE("TEMPLATE ERASED");
    UART_LINE("TRAINING REQUIRED");
    enter_training_wait();
}

/** Build the template from the three training vectors and store it. */
static void finish_training(void)
{
    const uint8_t *vectors[TRAINING_UTTERANCES];
    for (uint8_t i = 0; i < TRAINING_UTTERANCES - 1u; ++i) {
        vectors[i] = g_train[i];
    }
    vectors[TRAINING_UTTERANCES - 1u] = g_feature;

    vpl_template_average(vectors, TRAINING_UTTERANCES, g_template.features);
    g_template.threshold = vpl_threshold(vectors, TRAINING_UTTERANCES, g_template.features);

    for (uint8_t i = 0; i < TRAINING_UTTERANCES; ++i) {
        uart_puts_P(PSTR("TRAIN DISTANCE "));
        uart_put_u32(i + 1u);
        uart_puts_P(PSTR(" = "));
        uart_put_u32(vpl_distance(vectors[i], g_template.features));
        UART_LINE("");
    }

    template_save_to_eeprom(&g_template);
    UART_LINE("TEMPLATE SAVED");
    uart_kv(PSTR("THRESHOLD"), g_template.threshold);
    if (g_template.threshold >= THRESHOLD_MAX) {
        UART_LINE("WARNING: THRESHOLD CLAMPED - TRAINING UTTERANCES DISAGREED");
    }
}

/* -------------------------------------------------------------- states */

static void on_boot(void)
{
    UART_LINE("");
    UART_LINE("BOOT: VOICE PARKING LOCK");
    uart_kv(PSTR("SAMPLE RATE HZ"), SAMPLE_RATE_HZ);
    uart_kv(PSTR("CAPTURE FRAMES"), CAPTURE_FRAMES);
    uart_kv(PSTR("FEATURES"), FEATURE_COUNT);
#if SUCCESS_AUDIO_IS_PLACEHOLDER
    UART_LINE("AUDIO: PLACEHOLDER TEST TONE, NOT THE PERSIAN MESSAGE");
#endif

    if (template_load_from_eeprom(&g_template)) {
        UART_LINE("EEPROM VALID");
        uart_kv(PSTR("THRESHOLD"), g_template.threshold);
        enter_idle();
    } else {
        UART_LINE("EEPROM INVALID");
        UART_LINE("TRAINING REQUIRED");
        g_train_count = 0;
        enter_training_wait();
    }
}

static void on_training_listen(void)
{
    pump_frames();

    if (!g_capture_done) {
        if (systick_elapsed(g_state_since) >= LISTEN_TIMEOUT_MS) {
            UART_LINE("TIMEOUT - NO SPEECH DETECTED");
            enter_training_wait();
        }
        return;
    }

    adc_sampling_stop();
    UART_LINE("SPEECH END");
    report_capture_quality();
    feature_finalize(g_feature);

    if (g_train_count < TRAINING_UTTERANCES - 1u) {
        memcpy(g_train[g_train_count], g_feature, FEATURE_COUNT);
    }
    ++g_train_count;

    if (g_train_count >= TRAINING_UTTERANCES) {
        finish_training();
        enter_idle();
    } else {
        enter_training_wait();
    }
}

static void on_processing(void)
{
    adc_sampling_stop();
    feature_finalize(g_feature);
    report_capture_quality();

    const uint32_t score = vpl_distance(g_feature, g_template.features);
    uart_kv(PSTR("MATCH SCORE"), score);
    uart_kv(PSTR("THRESHOLD"), g_template.threshold);

    if (score <= g_template.threshold) {
        enter(SystemState::ACCEPTED);
    } else {
        UART_LINE("ACCESS DENIED");
        leds_set(false, true);
        enter(SystemState::REJECTED);
    }
}

static void on_accepted(void)
{
    UART_LINE("ACCESS GRANTED");
    leds_set(true, false);

    servo_unlock();
    UART_LINE("SERVO OPEN");

    /* Timer1 drives the servo in hardware and Timer2 drives the DAC, so the
     * gate holding its angle and the message playing are genuinely concurrent
     * - neither needs the CPU while it happens. */
    audio_play_success();
    UART_LINE("AUDIO START");

    g_audio_end_reported = false;
    enter(SystemState::GATE_OPEN);
}

static void on_gate_open(void)
{
    if (!g_audio_end_reported && !audio_is_playing()) {
        g_audio_end_reported = true;
        UART_LINE("AUDIO END");
    }
    if (systick_elapsed(g_state_since) < GATE_OPEN_MS) {
        return;
    }
    audio_stop();
    servo_lock();
    UART_LINE("SERVO CLOSED");
    enter_idle();
}

static void on_rejected(void)
{
    /* The message and the LED were set on the way in, so this state is purely
     * "hold the red LED long enough for a human to see it". */
    if (systick_elapsed(g_state_since) >= REJECT_INDICATE_MS) {
        enter_idle();
    }
}

/* ----------------------------------------------------------------- main */

int main(void)
{
    systick_init();
    uart_init();
    led_init();
    button_init();
    servo_pwm_init();
    dac_init();
    timer2_init();
    adc_init();
    sei();

    /* g_state starts at BOOT, so the first pass through the loop runs
     * on_boot() and immediately leaves for IDLE or TRAINING_WAIT. */
    for (;;) {
        const ButtonEvent btn = button_poll();
        if (btn.long_press) {
            erase_template_and_retrain();
            continue;
        }

        switch (g_state) {
        case SystemState::BOOT:
            on_boot();
            break;

        case SystemState::TRAINING_WAIT:
            if (btn.short_press) {
                start_listening();
                uart_puts_P(PSTR("TRAINING SAMPLE "));
                uart_put_u32(g_train_count + 1u);
                uart_puts_P(PSTR("/"));
                uart_put_u32(TRAINING_UTTERANCES);
                UART_LINE(" - SPEAK NOW");
                enter(SystemState::TRAINING_LISTEN);
            }
            break;

        case SystemState::TRAINING_LISTEN:
            on_training_listen();
            break;

        case SystemState::IDLE:
            pump_frames();
            if (g_capturing) {
                enter(SystemState::LISTENING);
            }
            break;

        case SystemState::LISTENING:
            pump_frames();
            if (g_capture_done) {
                UART_LINE("SPEECH END");
                enter(SystemState::PROCESSING);
            }
            break;

        case SystemState::PROCESSING:
            on_processing();
            break;

        case SystemState::ACCEPTED:
            on_accepted();
            break;

        case SystemState::GATE_OPEN:
            on_gate_open();
            break;

        case SystemState::REJECTED:
            on_rejected();
            break;
        }
    }
}
