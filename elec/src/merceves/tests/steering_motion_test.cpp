// Run: g++ -std=c++20 -Wall -Wextra -Werror tests/steering_motion_test.cpp -o /tmp/steering_motion_test && /tmp/steering_motion_test
#include "../steering_motion.hpp"

#include <cassert>
#include <cstdio>
#include <random>

constexpr int RANGE = 2000;
constexpr float MAX_SPEED = 17500.0f;
constexpr float ACCELERATION = 40000.0f;

static int finish(SteeringMotion& motion, int expected) {
    int count = 0;
    while (true) {
        const auto step = motion.next_step();
        if (step.direction == 0) {
            break;
        }
        assert(++count < 20000);
        assert(step.interval_us > 0);
        assert(std::isfinite(step.end_speed) && step.end_speed >= 0 && step.end_speed <= MAX_SPEED);
        const int before = motion.position();
        motion.step_taken(step);
        assert(motion.position() == before + step.direction);
        assert(std::abs(motion.position()) <= RANGE);
    }
    assert(motion.position() == expected);
    return count;
}

int main() {
    SteeringMotion motion(RANGE, MAX_SPEED, ACCELERATION);
    assert(finish(motion, 0) == 0);
    motion.set_target(1.0f);
    float previous_speed = 0.0f;
    bool braking = false;
    for (int i = 0; i < RANGE; ++i) {
        // Repeated commands must not restart the ramp.
        motion.set_target(1.0f);
        const auto step = motion.next_step();
        assert(step.direction == 1);
        if (step.end_speed < previous_speed) {
            braking = true;
        }
        if (braking) {
            assert(step.end_speed <= previous_speed);
        }
        const float energy_change = step.end_speed * step.end_speed - previous_speed * previous_speed;
        assert(std::fabs(energy_change) <= 2 * ACCELERATION + 32.0f);
        previous_speed = step.end_speed;
        motion.step_taken(step);
    }
    assert(braking && previous_speed == 0.0f);
    assert(finish(motion, RANGE) == 0);
    motion.set_target(-2.0f);  // Clamp to the other travel limit.
    assert(finish(motion, -RANGE) == 2 * RANGE);
    motion.set_target(0.0f);
    assert(finish(motion, 0) == RANGE);
    motion.set_target(1.0f / RANGE);
    assert(finish(motion, 1) == 1);

    // Reverse during acceleration: the direction must change at zero speed.
    motion.set_target(1.0f);
    for (int i = 0; i < 300; ++i) {
        motion.step_taken(motion.next_step());
    }
    motion.set_target(-1.0f);
    previous_speed = 1.0f;
    int previous_direction = 1;
    for (int i = 0; i < 5000; ++i) {
        const auto step = motion.next_step();
        if (!step.direction) {
            break;
        }
        if (step.direction != previous_direction) {
            assert(previous_speed == 0.0f);
        }
        previous_direction = step.direction;
        previous_speed = step.end_speed;
        motion.step_taken(step);
    }
    assert(finish(motion, -RANGE) == 0);

    // Timeout cancels motion without losing the count; restart uses that count.
    motion.set_target(1.0f);
    for (int i = 0; i < 400; ++i) {
        motion.step_taken(motion.next_step());
    }
    const int stopped_position = motion.position();
    motion.stop();
    assert(finish(motion, stopped_position) == 0);
    motion.set_target(0.0f);
    finish(motion, 0);

    // Targets changing inside the stopping distance, near limits, and at rest.
    std::mt19937 random(42);
    previous_speed = 0.0f;
    previous_direction = 1;
    for (int trial = 0; trial < 1000; ++trial) {
        const int target = static_cast<int>(random() % (2 * RANGE + 1)) - RANGE;
        motion.set_target(static_cast<float>(target) / RANGE);
        for (unsigned i = 0, steps = random() % 500; i < steps; ++i) {
            const auto step = motion.next_step();
            if (!step.direction) {
                break;
            }
            if (step.direction != previous_direction) {
                assert(previous_speed == 0.0f);
            }
            const float energy_change = step.end_speed_squared - previous_speed * previous_speed;
            assert(std::fabs(energy_change) <= 2 * ACCELERATION + 32.0f);
            previous_speed = step.end_speed;
            previous_direction = step.direction;
            motion.step_taken(step);
            assert(std::abs(motion.position()) <= RANGE);
        }
        if (trial % 10 == 0) {
            finish(motion, target);
            previous_speed = 0.0f;
        }
    }
    motion.set_target(0.0f);
    finish(motion, 0);

    // A longer configurable range must reach and sustain the velocity cap.
    SteeringMotion long_move(20000, MAX_SPEED, ACCELERATION);
    long_move.set_target(1.0f);
    bool at_max = false;
    for (int i = 0; i < 20000; ++i) {
        const auto step = long_move.next_step();
        assert(step.direction == 1 && step.end_speed <= MAX_SPEED);
        at_max |= step.end_speed == MAX_SPEED;
        long_move.step_taken(step);
    }
    assert(at_max && long_move.position() == 20000 && long_move.next_step().direction == 0);
    std::puts("Steering motion tests passed");
}
