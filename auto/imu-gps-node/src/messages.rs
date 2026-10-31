use serde::Serialize;

#[derive(Debug, Clone, Serialize)]
pub struct ImuData {
    pub accel_x: f64,
    pub accel_y: f64,
    pub accel_z: f64,
    pub gyro_x: f64,
    pub gyro_y: f64,
    pub gyro_z: f64,
}

#[derive(Debug, Clone, Serialize)]
pub struct GpsPosition {
    pub latitude: f64,
    pub longitude: f64,
}

#[derive(Debug, Clone, Serialize)]
pub struct MagnetometerData {
    pub mag_x: f64,
    pub mag_y: f64,
    pub mag_z: f64,
}
