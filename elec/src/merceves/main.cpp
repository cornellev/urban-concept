#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

#include "steering_motion.hpp"

// ---- pin map ----

struct DrivetrainMotor {
    uint pwm;
    uint fwd;
    uint back;
};

struct SteeringMotor {
    uint pulse;
    uint dir;
    uint enable;
};

constexpr DrivetrainMotor drivetrain[] = {
    {.pwm = 6, .fwd = 8, .back = 9},
    {.pwm = 10, .fwd = 12, .back = 13},
};

constexpr SteeringMotor steering = {.pulse = 14, .dir = 15, .enable = 16};

constexpr uint ONBOARD_LED_PIN = 25;

constexpr float DRIVETRAIN_PWM_FREQ_HZ = 2'000.0f;
constexpr float STEPPER_MAX_FREQ_HZ = 17'500.0f;
constexpr int32_t STEERING_HALF_RANGE_STEPS = 2'000;  // +/-90 degrees at 8,000 pulses/rev
constexpr float STEPPER_ACCELERATION = 40'000.0f;  // steps/s^2; applies to acceleration and braking
constexpr uint STEPPER_PULSE_US = 3;  // DM542T: pulse high and low must each last at least 2.5 us
constexpr uint STEPPER_DIR_SETUP_US = 5;     // DM542T: dir must lead the pulse edge by at least 5 us
constexpr uint STEPPER_ENABLE_SETUP_US = 5;  // DM542T: enable must lead dir by at least 5 us

static_assert(STEERING_HALF_RANGE_STEPS > 0);
static_assert(STEPPER_ACCELERATION > 0.0f);
static_assert(STEPPER_MAX_FREQ_HZ > 0.0f);
static_assert(STEPPER_MAX_FREQ_HZ * (2 * STEPPER_PULSE_US) < 1'000'000.0f);

constexpr uint32_t DRIVETRAIN_WRAP = SYS_CLK_HZ / DRIVETRAIN_PWM_FREQ_HZ - 1;
static_assert(DRIVETRAIN_WRAP <= UINT16_MAX, "drivetrain PWM frequency too low for clkdiv = 1");

static SteeringMotion steering_motion(STEERING_HALF_RANGE_STEPS, STEPPER_MAX_FREQ_HZ, STEPPER_ACCELERATION);
static SteeringMotion::Step pending_step;
static uint64_t next_step_us = 0;

// Count GPIO pulse edges directly so the position tracks pulses actually sent to the driver.
static void init_motor(const SteeringMotor& motor) {
    for (uint pin : {motor.enable, motor.dir, motor.pulse}) {
        gpio_init(pin);
        gpio_put(pin, 0);
        gpio_set_dir(pin, GPIO_OUT);
    }
}

static void stop_steering() {
    pending_step = {};
    steering_motion.stop();
    gpio_put(steering.pulse, 0);
    gpio_put(steering.enable, 0);
}

// Called continuously, independently of serial command arrival. Late pulses are never
// caught up in a burst: serial/interrupt delays can only make the motion slower.
static void update_steering() {
    uint64_t last_edge_us = 0;
    if (pending_step.direction != 0 && time_us_64() >= next_step_us) {
        last_edge_us = time_us_64();
        gpio_put(steering.pulse, 1);
        steering_motion.step_taken(pending_step);
        sleep_us(STEPPER_PULSE_US);
        gpio_put(steering.pulse, 0);
        pending_step = {};
    }
    if (pending_step.direction != 0) {
        return;
    }
    pending_step = steering_motion.next_step();
    if (pending_step.direction == 0) {
        return;  // Keep the driver enabled to hold the requested position.
    }
    if (!gpio_get_out_level(steering.enable)) {
        gpio_put(steering.enable, 1);
        sleep_us(STEPPER_ENABLE_SETUP_US);
    }
    const bool reverse = pending_step.direction < 0;
    if (gpio_get_out_level(steering.dir) != reverse) {
        gpio_put(steering.dir, reverse);
        sleep_us(STEPPER_DIR_SETUP_US);
    }
    const uint64_t now = time_us_64();
    next_step_us = std::max(last_edge_us != 0 ? last_edge_us + pending_step.interval_us
                                            : now + pending_step.interval_us,
                            now + STEPPER_PULSE_US);
}

// pwm is fixed frequency, variable duty. starts forward at 0% duty
static void init_motor(const DrivetrainMotor& motor) {
    gpio_init(motor.fwd);
    gpio_put(motor.fwd, 1);
    gpio_set_dir(motor.fwd, GPIO_OUT);

    gpio_init(motor.back);
    gpio_put(motor.back, 0);
    gpio_set_dir(motor.back, GPIO_OUT);

    uint slice = pwm_gpio_to_slice_num(motor.pwm);
    // The shared SDK mapper can return slices beyond this chip's PWM array.
    hard_assert(slice < NUM_PWM_SLICES);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, DRIVETRAIN_WRAP);
    pwm_init(slice, &cfg, false);
    pwm_set_gpio_level(motor.pwm, 0);
    gpio_set_function(motor.pwm, GPIO_FUNC_PWM);
    pwm_set_enabled(slice, true);
}

// [-1.0, 1.0]; the sign picks fwd or back (always opposite), the magnitude maps linearly onto duty
static void set_speed(const DrivetrainMotor& motor, float speed) {
    speed = std::clamp(speed, -1.0f, 1.0f);
    bool reverse = speed < 0.0f;

    // cut power before swapping direction, and write both pins at once so they're never equal
    if (gpio_get_out_level(motor.back) != reverse) {
        pwm_set_gpio_level(motor.pwm, 0);
        gpio_put_masked((1u << motor.fwd) | (1u << motor.back), reverse ? 1u << motor.back : 1u << motor.fwd);
    }

    // level = wrap + 1 holds the output high the whole period
    pwm_set_gpio_level(motor.pwm, static_cast<uint16_t>(std::fabs(speed) * (DRIVETRAIN_WRAP + 1)));
}

static void stop_all() {
    stop_steering();
    for (const DrivetrainMotor& motor : drivetrain) {
        set_speed(motor, 0.0f);
    }
}

// ---- serial protocol ----
// every frame is COBS-encoded and ends in a 0x00 byte, so a garbled or dropped byte only loses one frame
// decoded frame: [type u8][payload][crc u16], crc is CRC-16/CCITT-FALSE over type + payload
// all multi-byte values are little-endian
//   jetson -> pico  MSG_DRIVE      0x01  f32 steering position [-1, 1], f32 drivetrain speed [-1, 1]
// steering: -1 = -half range, 0 = boot position (center), +1 = +half range
//   pico -> jetson  MSG_TELEMETRY  0x02  f32 left rpm, f32 right rpm

constexpr uint8_t MSG_DRIVE = 0x01;
constexpr uint8_t MSG_TELEMETRY = 0x02;
constexpr size_t DRIVE_FRAME_LEN = 1 + 2 * sizeof(float) + 2;
constexpr size_t MAX_ENCODED_LEN = 32;     // longer frames are garbage and get dropped
constexpr uint32_t COMMAND_TIMEOUT_MS = 200;  // stop every motor if no valid command arrives for this long

static uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
        }
    }
    return crc;
}

// out must hold len bytes. returns the decoded length, or 0 if the frame is malformed
static size_t cobs_decode(const uint8_t* in, size_t len, uint8_t* out) {
    size_t read = 0;
    size_t written = 0;
    while (read < len) {
        uint8_t code = in[read++];
        if (code == 0 || read + code - 1 > len) {
            return 0;
        }
        for (uint8_t i = 1; i < code; i++) {
            out[written++] = in[read++];
        }
        if (code < 0xFF && read < len) {
            out[written++] = 0;
        }
    }
    return written;
}

// out must hold len + len / 254 + 1 bytes. returns the encoded length, without the trailing 0x00
static size_t cobs_encode(const uint8_t* in, size_t len, uint8_t* out) {
    size_t code_at = 0;
    size_t written = 1;
    uint8_t code = 1;
    for (size_t i = 0; i < len; i++) {
        if (in[i] != 0) {
            out[written++] = in[i];
            code++;
        }
        if (in[i] == 0 || code == 0xFF) {
            out[code_at] = code;
            code_at = written++;
            code = 1;
        }
    }
    out[code_at] = code;
    return written;
}

// not called yet; for when the rpm sensors are wired up
[[maybe_unused]] static void send_telemetry(float left_rpm, float right_rpm) {
    uint8_t frame[1 + 2 * sizeof(float) + 2];
    frame[0] = MSG_TELEMETRY;
    memcpy(&frame[1], &left_rpm, sizeof(float));
    memcpy(&frame[5], &right_rpm, sizeof(float));
    uint16_t crc = crc16(frame, 9);
    frame[9] = crc & 0xFF;
    frame[10] = crc >> 8;

    uint8_t encoded[sizeof(frame) + 2];
    size_t len = cobs_encode(frame, sizeof(frame), encoded);
    encoded[len++] = 0;

    // raw, since stdio would turn any 0x0A byte into 0x0D 0x0A
    for (size_t i = 0; i < len; i++) {
        putchar_raw(encoded[i]);
    }
}

// returns true if the frame was a valid drive command, which is then applied
static bool handle_frame(const uint8_t* encoded, size_t len) {
    uint8_t frame[MAX_ENCODED_LEN];
    size_t n = cobs_decode(encoded, len, frame);
    if (n != DRIVE_FRAME_LEN || frame[0] != MSG_DRIVE) {
        return false;
    }
    uint16_t crc = frame[n - 2] | (frame[n - 1] << 8);
    if (crc16(frame, n - 2) != crc) {
        return false;
    }

    float steering_position;
    float drivetrain_speed;
    memcpy(&steering_position, &frame[1], sizeof(float));
    memcpy(&drivetrain_speed, &frame[5], sizeof(float));
    if (!std::isfinite(steering_position) || !std::isfinite(drivetrain_speed)) {
        return false;
    }

    steering_motion.set_target(steering_position);
    for (const DrivetrainMotor& motor : drivetrain) {
        set_speed(motor, drivetrain_speed);
    }
    return true;
}

int main() {
    stdio_init_all();

    gpio_init(ONBOARD_LED_PIN);
    gpio_set_dir(ONBOARD_LED_PIN, GPIO_OUT);

    init_motor(steering);
    init_motor(drivetrain[0]);
    init_motor(drivetrain[1]);
    stop_all();

    uint8_t rx[MAX_ENCODED_LEN];
    size_t rx_len = 0;
    bool rx_overflow = false;
    absolute_time_t last_command = get_absolute_time();
    bool timed_out = true;

    while (true) {
        int c = getchar_timeout_us(0);
        if (c >= 0) {
            if (c == 0) {
                if (!rx_overflow && handle_frame(rx, rx_len)) {
                    last_command = get_absolute_time();
                    timed_out = false;
                }
                rx_len = 0;
                rx_overflow = false;
            } else if (rx_len < MAX_ENCODED_LEN) {
                rx[rx_len++] = c;
            } else {
                rx_overflow = true;
            }
        }

        if (!timed_out && absolute_time_diff_us(last_command, get_absolute_time()) > COMMAND_TIMEOUT_MS * 1000) {
            stop_all();
            timed_out = true;
        }

        if (!timed_out) {
            update_steering();
        }

        // led on while commands are arriving
        gpio_put(ONBOARD_LED_PIN, !timed_out);
    }
}
