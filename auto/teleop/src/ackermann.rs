use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct AckermannCommand {
    pub steering_angle: f64,
    pub speed: f64,
}

impl AckermannCommand {
    pub fn new(steering_angle: f64, speed: f64) -> AckermannCommand {
        AckermannCommand { steering_angle, speed }
    }

    pub fn default() -> AckermannCommand {
        AckermannCommand { steering_angle: 0.0, speed: 0.0 }
    }

    pub fn update(&mut self, steering_angle: f64, speed: f64) {
        self.steering_angle = steering_angle;
        self.speed = speed;
    }
}