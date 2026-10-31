mod messages;
mod imu;

use crate::imu::Imu;
use std::{thread, time::Duration};

fn main() {
    let mut imu = Imu::new(
            "/dev/spidev0.0"
        ).expect("Failed to initialize IMU");
    let period = Duration::from_millis(100);
    loop {
        match imu.read() {
            Ok(data) => {
                println!("{data:?}");
            }
            Err(e) => {
                eprintln!("IMU: {e}");
            }
        }
        thread::sleep(period);
    }
}
