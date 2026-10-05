/* Voice-Authenticated Smart Parking Lock - student starter.
 *
 * Arduino Uno R3 / ATmega328P, built WITHOUT the Arduino framework so that the
 * firmware owns all three timers outright. There is no millis(), no delay(),
 * no Serial and no analogRead() here; you write the register-level equivalents.
 *
 *   Timer0  1 ms system tick          systick.cpp   TODO
 *   Timer1  50 Hz servo PWM (OC1A)    servo.cpp     TODO
 *   Timer2  8 kHz ADC / DAC clock     timer2.cpp    TODO
 *
 * The one rule that shapes everything else: the main loop must never block.
 * Every duration in this system - the 1.6 s capture, the 3 s gate-open, the
 * 1 s reject indication, the audio clip - is a deadline compared against
 * systick_ms(), never a busy-wait. If you find yourself wanting delay(), stop
 * and add a state instead.
 *
 * What is finished for you: the enum, the LED and button plumbing, the boot
 * sequence, and the dispatch loop. What is not: everything the states actually
 * do. See ASSIGNMENT.md.
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

/* ------------------------------------------------------ provided: LEDs */

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

/* ---------------------------------------------------- provided: button */

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
 * Provided complete so you can spend your time on the peripherals. Note that
 * it depends on systick_ms() actually advancing - until you implement Timer0
 * this returns nothing at all, which is a useful first thing to get working.
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

/* ------------------------------------------------------ provided: enter */

static void enter(SystemState s)
{
    g_state = s;
    g_state_since = systick_ms();
}

/* --------------------------------------------------------- TODO: pump */

/* TODO(sampling + state machine): drain the ADC's completed frames.
 *
 * This runs on every pass of the main loop and is where the interrupt-driven
 * front end meets the state machine:
 *
 *   - if a frame is not ready, return immediately;
 *   - if not capturing, compute the frame magnitude and feed the VAD; when it
 *     reports START, reset the extractor and begin capturing;
 *   - if capturing, fold the frame into the extractor; when it reports the
 *     capture window is full, stop capturing and set g_capture_done;
 *   - release the frame so the interrupt can reuse the buffer.
 *
 * Print "SPEECH START" here, on the transition, so that both IDLE and
 * TRAINING_LISTEN get it without duplicating the logic. It is the only print
 * allowed inside the capture path: 14 bytes at 115200 baud is 1.2 ms of the
 * 8 ms frame budget, and a second line would not fit alongside the DSP.
 *
 * One detail worth thinking about: the VAD only confirms speech several frames
 * after it actually started. The frame that triggered START is still in your
 * hand - is it part of the utterance or not? What do you lose either way?
 */
static void pump_frames(void)
{
    /* TODO */
}

/* ------------------------------------------------------- TODO: states */

/* Every state below must produce the UART trace listed in ASSIGNMENT.md. The
 * exact strings matter: the grading script greps for them. */

static void start_listening(void)
{
    /* TODO: clear the capture flags, reset the extractor and the VAD, clear
     * the overrun counter, and start sampling. */
}

static void enter_training_wait(void)
{
    /* TODO: stop sampling, LEDs off, prompt for training utterance
     * (g_train_count + 1) of TRAINING_UTTERANCES, and enter TRAINING_WAIT. */
    enter(SystemState::TRAINING_WAIT);
}

static void enter_idle(void)
{
    /* TODO: LEDs off, start listening, print "IDLE - LISTENING FOR PASSPHRASE",
     * and enter IDLE. */
    enter(SystemState::IDLE);
}

static void erase_template_and_retrain(void)
{
    /* TODO(eeprom): long press. Stop audio and sampling, close the gate,
     * invalidate the stored template, reset g_train_count, print
     * "TEMPLATE ERASED" and "TRAINING REQUIRED", then go to TRAINING_WAIT. */
    (void)g_template;
    (void)g_train_count;
    enter_training_wait();
}

static void finish_training(void)
{
    /* TODO(training): the third utterance has just been captured.
     *
     * Build the array of TRAINING_UTTERANCES vectors - the first
     * TRAINING_UTTERANCES-1 are in g_train, the last is g_feature - average
     * them into g_template.features, derive g_template.threshold from them,
     * write the record to EEPROM, and print "TEMPLATE SAVED" and the
     * threshold.
     *
     * Print each training vector's distance from the finished template too.
     * Those three numbers are the single most useful diagnostic in the whole
     * system: if they are large, your recordings disagree and no threshold
     * will save you. */
    (void)g_train;
    (void)g_feature;
}

static void on_boot(void)
{
    leds_set(false, false);
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
    /* TODO(training): pump frames; on LISTEN_TIMEOUT_MS with no speech, print
     * "TIMEOUT - NO SPEECH DETECTED" and go back to TRAINING_WAIT.
     *
     * When the capture completes: stop sampling, print "SPEECH END", report any
     * ADC overruns, finalise the features, store them (or keep them in
     * g_feature if this is the last one), and either finish training or ask for
     * the next utterance. */
    pump_frames();
    (void)g_capture_done;
    (void)LISTEN_TIMEOUT_MS;
    finish_training();
}

static void on_processing(void)
{
    /* TODO(matching): stop sampling, finalise the features into g_feature,
     * score them against g_template.features, print "MATCH SCORE = ..." and
     * "THRESHOLD = ...", and go to ACCEPTED or REJECTED.
     *
     * REJECTED also prints "ACCESS DENIED" and lights the red LED on the way
     * in - do it here, on the transition, not inside the state, or you will
     * print it once per loop iteration. */
    (void)g_feature;
    enter(SystemState::REJECTED);
}

static void on_accepted(void)
{
    /* TODO(pwm + dac): print "ACCESS GRANTED", green LED on, servo_unlock()
     * and print "SERVO OPEN", audio_play_success() and print "AUDIO START",
     * then go to GATE_OPEN.
     *
     * Timer1 drives the servo in hardware and Timer2 drives the DAC, so the
     * gate holding its angle and the message playing are genuinely concurrent.
     * Neither should need the CPU while it happens. */
    enter(SystemState::GATE_OPEN);
}

static void on_gate_open(void)
{
    /* TODO: print "AUDIO END" once, when playback finishes. After GATE_OPEN_MS
     * from entering this state, stop the audio, servo_lock(), print
     * "SERVO CLOSED" and return to IDLE. */
    enter_idle();
}

static void on_rejected(void)
{
    /* TODO: hold the red LED for REJECT_INDICATE_MS, then return to IDLE. */
    enter_idle();
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
                /* TODO: start_listening(), announce which sample is being
                 * recorded, and enter TRAINING_LISTEN. */
                start_listening();
                enter(SystemState::TRAINING_LISTEN);
            }
            break;

        case SystemState::TRAINING_LISTEN:
            on_training_listen();
            break;

        case SystemState::IDLE:
            /* TODO: pump frames; when the VAD starts a capture, move to
             * LISTENING. */
            pump_frames();
            (void)g_capturing;
            break;

        case SystemState::LISTENING:
            /* TODO: pump frames; when the capture completes, print
             * "SPEECH END" and move to PROCESSING. */
            pump_frames();
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
