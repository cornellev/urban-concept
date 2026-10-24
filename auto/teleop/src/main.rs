//! Drive the car from a gamepad: left stick x steers, right stick y drives (up is forward).
//! The triggers aren't used: on our controller, LT wraps from 255 back to 0 when fully pressed.
//!
//! usage: teleop [--dry-run] [serial port]
//! --dry-run prints the commands instead of sending them.

mod protocol;

use std::io::Write;
use std::thread;
use std::time::{Duration, Instant};

use gilrs::{Axis, EventType, Gilrs};
use serialport::SerialPort;

const DEFAULT_PORT: &str = "/dev/serial/by-id/usb-Raspberry_Pi_Pico_505374175A8A7018-if00";
const SEND_PERIOD: Duration = Duration::from_millis(20); // 50 Hz, well inside the firmware's 200 ms timeout
const REOPEN_PERIOD: Duration = Duration::from_secs(1);

const STICK_DEADZONE: f32 = 0.1;
const MAX_STEERING: f32 = 1.0;
const MAX_DRIVETRAIN: f32 = 0.3; // keep it slow while testing

// zero inside the deadzone, then rescaled so the output still reaches 1.0 at full stick
fn deadzone(value: f32) -> f32 {
    if value.abs() < STICK_DEADZONE {
        0.0
    } else {
        (value - STICK_DEADZONE * value.signum()) / (1.0 - STICK_DEADZONE)
    }
}

fn open_port(path: &str) -> Option<Box<dyn SerialPort>> {
    match serialport::new(path, 115_200)
        .timeout(Duration::from_millis(100))
        .open()
    {
        Ok(port) => {
            eprintln!("opened {path}");
            Some(port)
        }
        Err(e) => {
            eprintln!("can't open {path}: {e}");
            None
        }
    }
}

fn main() {
    let mut dry_run = false;
    let mut port_path = DEFAULT_PORT.to_string();
    for arg in std::env::args().skip(1) {
        if arg == "--dry-run" {
            dry_run = true;
        } else {
            port_path = arg;
        }
    }

    let mut gilrs = Gilrs::new().expect("failed to start gamepad input");
    for (_, pad) in gilrs.gamepads() {
        eprintln!("gamepad: {}", pad.name());
    }
    let mut port: Option<Box<dyn SerialPort>> = None;
    let mut last_open_attempt: Option<Instant> = None;

    loop {
        // drain events so gilrs's cached gamepad state is current
        while let Some(event) = gilrs.next_event() {
            match event.event {
                EventType::Connected => {
                    eprintln!("gamepad connected: {}", gilrs.gamepad(event.id).name())
                }
                EventType::Disconnected => eprintln!("gamepad disconnected, stopping"),
                _ => {}
            }
        }

        // no gamepad means stop, but keep sending so the link stays up
        let (steering, drivetrain) = match gilrs.gamepads().next() {
            Some((_, pad)) => {
                let steering = deadzone(pad.value(Axis::LeftStickX)) * MAX_STEERING;
                let drivetrain = deadzone(pad.value(Axis::RightStickY)) * MAX_DRIVETRAIN;
                (steering, drivetrain)
            }
            None => (0.0, 0.0),
        };

        if dry_run {
            println!("steering {steering:+.2}  drivetrain {drivetrain:+.2}");
        } else {
            if port.is_none() && last_open_attempt.is_none_or(|t| t.elapsed() >= REOPEN_PERIOD) {
                last_open_attempt = Some(Instant::now());
                port = open_port(&port_path);
            }
            if let Some(p) = port.as_mut()
                && let Err(e) = p.write_all(&protocol::encode_drive(steering, drivetrain))
            {
                eprintln!("write failed, reopening: {e}");
                port = None;
            }
        }

        thread::sleep(SEND_PERIOD);
    }
}
