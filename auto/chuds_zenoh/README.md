# chuds_zenoh

Bridges Zenoh and the chuds CAN bus on the car's Linux computer. Every payload is JSON.

| Direction   | Zenoh key                      | Payload                                  | chuds                                  |
| ----------- | ------------------------------ | ---------------------------------------- | -------------------------------------- |
| Zenoh → CAN | `cev/commands/control`         | `{"steering_angle": deg, "velocity": m/s}` | `merceves::DriveCommand` on `0x150`  |
| CAN → Zenoh | `cev/sensors/wheels/left_rpm`  | `{"rpm": rotations/s}`                   | `merceves::Telemetry` from `0x250`     |
| CAN → Zenoh | `cev/sensors/wheels/right_rpm` | `{"rpm": rotations/s}`                   | `merceves::Telemetry` from `0x250`     |
| CAN → Zenoh | `cev/sensors/steering_angle`   | `{"steering_angle": deg}`                | `merceves::Telemetry` from `0x250`     |
| CAN → Zenoh | `cev/sensors/imu`              | `{"accel_x", "accel_y", "accel_z": m/s^2, "roll_rate", "pitch_rate", "yaw_rate": rad/s}`, x forward, y left, z up | TBD |

A control payload that is not JSON, or lacks either field as a number, is dropped with a line on
stderr. The node only converts and forwards: it has no clamps and no timeout.

## Build

The node needs Linux and an installed zenoh-c and zenoh-cpp, found with `find_package`. The tests
need neither, so `just test` runs on any machine.

```sh
just build
./build/chuds_zenoh            # uses can0
./build/chuds_zenoh -i vcan0
```
