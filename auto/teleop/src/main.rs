//! Drive the car from a gamepad: left stick x steers, right stick y drives (up is forward).
//! The triggers aren't used: on our controller, LT wraps from 255 back to 0 when fully pressed.
//! Shows the wheel rpm the pico reports.
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

const MAX_FRAME_LEN: usize = 32; // longer runs without a 0x00 are garbage

const STICK_DEADZONE: f32 = 0.1;
const MAX_STEERING: f32 = 1.0;
const MAX_DRIVETRAIN: f32 = 0.6; // keep it slow while testing

// zero inside the deadzone, then rescaled so the output still reaches 1.0 at full stick
fn deadzone(value: f32) -> f32 {
    if value.abs() < STICK_DEADZONE {
        0.0
    } else {
        (value - STICK_DEADZONE * value.signum()) / (1.0 - STICK_DEADZONE)
    }
}

fn open_port(path: &str) -> Option<Box<dyn SerialPort>> {
    // the pico only reads usb input while dtr is set; linux sets it on open, windows doesn't
    match serialport::new(path, 115_200)
        .timeout(Duration::from_millis(100))
        .dtr_on_open(true)
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

// reads whatever has arrived without blocking, splits it into frames on 0x00,
// and returns the newest valid telemetry (left rpm, right rpm)
fn read_telemetry(
    port: &mut dyn SerialPort,
    rx: &mut Vec<u8>,
) -> std::io::Result<Option<(f32, f32)>> {
    let mut incoming = vec![0; port.bytes_to_read()? as usize];
    port.read_exact(&mut incoming)?;

    let mut latest = None;
    for byte in incoming {
        if byte == 0x00 {
            if let Some(rpm) = protocol::decode_telemetry(rx) {
                latest = Some(rpm);
            }
            rx.clear();
        } else if rx.len() < MAX_FRAME_LEN {
            rx.push(byte);
        } else {
            rx.clear(); // the rest of this frame then fails to decode and gets dropped
        }
    }
    Ok(latest)
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
    let mut rx = Vec::new();

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
                rx.clear();
            }
            if let Some(p) = port.as_mut()
                && let Err(e) = p.write_all(&protocol::encode_drive(steering, drivetrain))
            {
                eprintln!("write failed, reopening: {e}");
                port = None;
            }
            if let Some(p) = port.as_mut() {
                match read_telemetry(p.as_mut(), &mut rx) {
                    // one line, rewritten in place
                    Ok(Some((left, right))) => {
                        eprint!("\rrpm  left {left:7.1}  right {right:7.1}  ")
                    }
                    Ok(None) => {}
                    Err(e) => {
                        eprintln!("\nread failed, reopening: {e}");
                        port = None;
                    }
                }
            }
        }

        thread::sleep(SEND_PERIOD);
    }
}
