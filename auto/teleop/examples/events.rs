//! Debugging aid: records every gilrs event for a few seconds, then prints a summary per input.
//!
//! usage: cargo run --example events [seconds]

use std::collections::BTreeMap;
use std::time::{Duration, Instant};

use gilrs::{EventType, Gilrs};

#[derive(Default)]
struct Seen {
    count: usize,
    min: f32,
    max: f32,
    last: f32,
}

fn main() {
    let seconds: u64 = std::env::args()
        .nth(1)
        .and_then(|s| s.parse().ok())
        .unwrap_or(10);
    let mut gilrs = Gilrs::new().expect("failed to start gamepad input");
    for (_, pad) in gilrs.gamepads() {
        eprintln!(
            "gamepad: {} (mapping: {:?})",
            pad.name(),
            pad.mapping_source()
        );
    }
    eprintln!("recording for {seconds} s: press LT fully, release, press RT fully, release");

    let mut seen: BTreeMap<String, Seen> = BTreeMap::new();
    let end = Instant::now() + Duration::from_secs(seconds);
    while Instant::now() < end {
        while let Some(event) = gilrs.next_event() {
            let (key, value) = match event.event {
                EventType::ButtonChanged(button, value, code) => {
                    (format!("ButtonChanged {button:?} {code}"), value)
                }
                EventType::AxisChanged(axis, value, code) => {
                    (format!("AxisChanged   {axis:?} {code}"), value)
                }
                EventType::ButtonPressed(button, code) => {
                    (format!("ButtonPressed {button:?} {code}"), 1.0)
                }
                EventType::ButtonReleased(button, code) => {
                    (format!("ButtonReleased {button:?} {code}"), 0.0)
                }
                _ => continue,
            };
            let s = seen.entry(key).or_insert(Seen {
                min: f32::MAX,
                max: f32::MIN,
                ..Default::default()
            });
            s.count += 1;
            s.min = s.min.min(value);
            s.max = s.max.max(value);
            s.last = value;
        }
        std::thread::sleep(Duration::from_millis(1));
    }

    for (key, s) in &seen {
        println!(
            "{key:<55} n={:<5} min={:+.3} max={:+.3} last={:+.3}",
            s.count, s.min, s.max, s.last
        );
    }
}
