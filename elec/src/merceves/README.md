# Merceves steering

The steering float in `MSG_DRIVE` requests a position in `[-1, 1]`. With the
defaults, `-1` is -2,000 pulses, `0` is center, and `+1` is +2,000 pulses.
At 8,000 pulses per revolution, this is 180 degrees of total motor travel.

Configure these constants near the top of `main.cpp`:

- `STEERING_HALF_RANGE_STEPS`: pulses from center to either end (default 2,000).
- `STEPPER_MAX_FREQ_HZ`: maximum pulse rate (default 17,500 steps/s).
- `STEPPER_ACCELERATION`: acceleration and deceleration (default 40,000 steps/s²).

The controller counts emitted pulses and assumes the steering is centered at boot.
There is no homing or sensor correction, so missed steps or movement while the
driver is disabled are not reflected in the estimate. On arrival, the driver stays
enabled to hold position. A new target inside the stopping distance can be passed
while braking before the controller returns to it; travel stays within the configured
limits. Pulse timing is serviced by the main loop; loop/interrupt delays may reduce
the effective speed below the configured maximum.

Keep sending drive commands while moving or holding. The existing 200 ms command
timeout immediately cancels motion and disables the motors, preserving the step
count. The next valid command resumes from that count. The drivetrain float still
requests speed, and the serial frame layout is unchanged.

Run the host motion tests from this directory:

```sh
g++ -std=c++20 -Wall -Wextra -Werror tests/steering_motion_test.cpp -o /tmp/steering_motion_test
/tmp/steering_motion_test
```
