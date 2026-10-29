// Run: g++ -std=c++20 -Wall -Wextra -Werror tests/steering_pulses_test.cpp -o /tmp/steering_pulses_test && /tmp/steering_pulses_test
#include "../steering_pulses.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>

constexpr int RANGE = 2000;
constexpr float MAX_SPEED = 17500.0f;
constexpr float ACCELERATION = 40000.0f;
constexpr uint16_t PULSE_US = 3;
constexpr uint16_t IDLE_US = 1000;
constexpr uint16_t MAX_US = 65000;
constexpr uint64_t DIR_SETUP_US = 5;

// a steering PWM slice with double-buffered TOP and level, plus the dir pin the interrupt drives
struct SimulatedPwm {
    SteeringPulses& pulses;
    SteeringPulses::Period running = pulses.idle();
    SteeringPulses::Period buffered = pulses.idle();
    uint64_t now_us = 0;
    int dir = 1;
    uint64_t dir_changed_us = 0;
    std::vector<uint64_t> edges_us{};
    int32_t position = 0;  // what the driver saw

    // runs to the end of the current period, wraps, and runs the interrupt
    void period() {
        if (running.direction != 0) {
            const uint64_t edge = now_us + running.level;
            assert(running.direction == dir);
            assert(edge - dir_changed_us >= DIR_SETUP_US);
            assert(running.level >= PULSE_US);              // low part
            assert(running.top + 1u - running.level == PULSE_US);  // high part
            edges_us.push_back(edge);
            position += running.direction;
            assert(std::abs(position) <= RANGE);
        }
        now_us += running.top + 1u;
        running = buffered;
        const SteeringPulses::Wrap wrap = pulses.on_wrap();
        if (wrap.direction_now != 0 && wrap.direction_now != dir) {
            dir = wrap.direction_now;
            dir_changed_us = now_us;  // right at the wrap, after the previous pulse ended
        }
        buffered = wrap.next;
    }

    void run_until_idle() {
        int idle_periods = 0;
        for (int i = 0; idle_periods < 3; i++) {
            assert(i < 100000);
            period();
            idle_periods = running.direction == 0 && buffered.direction == 0 ? idle_periods + 1 : 0;
        }
    }
};

// the intervals SteeringMotion asks for when run on its own
static std::vector<uint32_t> reference_intervals(float target) {
    SteeringMotion motion(RANGE, MAX_SPEED, ACCELERATION);
    motion.set_target(target);
    std::vector<uint32_t> intervals;
    for (auto step = motion.next_step(); step.direction != 0; step = motion.next_step()) {
        intervals.push_back(step.interval_us);
        motion.step_taken(step);
    }
    return intervals;
}

int main() {
    // full travel: same pulse timing as the planner on its own, and the count lands on target
    {
        SteeringMotion motion(RANGE, MAX_SPEED, ACCELERATION);
        SteeringPulses pulses(motion, PULSE_US, IDLE_US, MAX_US);
        SimulatedPwm pwm{pulses};
        pulses.resume();
        motion.set_target(1.0f);
        pwm.run_until_idle();

        const auto expected = reference_intervals(1.0f);
        assert(pwm.edges_us.size() == expected.size());
        for (size_t i = 1; i < pwm.edges_us.size(); i++) {
            assert(pwm.edges_us[i] - pwm.edges_us[i - 1] == expected[i]);
        }
        assert(pwm.position == RANGE && pulses.position() == RANGE && motion.position() == RANGE);
    }

    // reversing mid-move: brakes, flips dir between pulses, and ends at the far limit
    {
        SteeringMotion motion(RANGE, MAX_SPEED, ACCELERATION);
        SteeringPulses pulses(motion, PULSE_US, IDLE_US, MAX_US);
        SimulatedPwm pwm{pulses};
        pulses.resume();
        motion.set_target(1.0f);
        while (pwm.position < RANGE / 2) {
            pwm.period();
        }
        motion.set_target(-1.0f);
        pwm.run_until_idle();
        assert(pwm.position == -RANGE && pulses.position() == -RANGE && motion.position() == -RANGE);
    }

    // stopping mid-move: scheduled pulses still go out, then nothing, and every count agrees
    {
        SteeringMotion motion(RANGE, MAX_SPEED, ACCELERATION);
        SteeringPulses pulses(motion, PULSE_US, IDLE_US, MAX_US);
        SimulatedPwm pwm{pulses};
        pulses.resume();
        motion.set_target(1.0f);
        while (pwm.position < 700) {
            pwm.period();
        }
        pulses.stop();
        int periods = 0;
        while (!pulses.drained()) {
            pwm.period();
            assert(++periods <= 3);
        }
        const auto edges = pwm.edges_us.size();
        for (int i = 0; i < 10; i++) {
            pwm.period();
        }
        assert(pwm.edges_us.size() == edges);
        assert(pwm.position == pulses.position() && pwm.position == motion.position());
        assert(pwm.position >= 700 && pwm.position <= 702);

        // resuming picks up from the counted position
        pulses.resume();
        motion.set_target(0.0f);
        pwm.run_until_idle();
        assert(pwm.position == 0 && pulses.position() == 0 && motion.position() == 0);
    }

    // holdoff after enabling: no pulse until it's over, and repeated resumes don't restart it
    {
        constexpr uint32_t HOLDOFF = 200;
        SteeringMotion motion(RANGE, MAX_SPEED, ACCELERATION);
        SteeringPulses pulses(motion, PULSE_US, IDLE_US, MAX_US);
        SimulatedPwm pwm{pulses};
        motion.set_target(1.0f);
        pulses.resume(HOLDOFF);
        for (uint32_t i = 0; i < HOLDOFF; i++) {
            pulses.resume(HOLDOFF);  // a command arrives every period
            pwm.period();
        }
        assert(pwm.edges_us.empty() && pwm.now_us == HOLDOFF * IDLE_US);
        pwm.run_until_idle();
        assert(pwm.edges_us.front() >= HOLDOFF * IDLE_US);
        assert(pwm.position == RANGE && pulses.position() == RANGE);
    }

    // stopped from the start: never pulses
    {
        SteeringMotion motion(RANGE, MAX_SPEED, ACCELERATION);
        SteeringPulses pulses(motion, PULSE_US, IDLE_US, MAX_US);
        SimulatedPwm pwm{pulses};
        motion.set_target(1.0f);
        for (int i = 0; i < 20; i++) {
            pwm.period();
        }
        assert(pwm.edges_us.empty() && pulses.drained());
    }

    std::puts("Steering pulse tests passed");
}
