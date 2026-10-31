use anyhow::{bail, Result};
use embedded_hal::spi::SpiDevice;
use linux_embedded_hal::spidev::{
    SpiModeFlags, SpidevOptions,
};
use linux_embedded_hal::SpidevDevice;

use crate::messages::ImuData;

pub struct Imu {
    spi: SpidevDevice,
}

impl Imu {
    pub fn new(path: &str) -> Result<Self> {
        let mut spi = SpidevDevice::open(path)?;

        spi.configure(
            &SpidevOptions::new()
                .bits_per_word(8)
                .max_speed_hz(10_000)
                .mode(SpiModeFlags::SPI_MODE_0)
                .build()
        )?;

        let mut imu = Self{spi};
        
        let mut chip_responding = false;

        while !chip_responding {
            let whoami = imu.read_reg(0x0F)?;
            if whoami != 0x6B {
                eprintln!("Who am I check failed for IMU. Instead returned : 0x{whoami:02X}. Trying again...");
            } else {
                chip_responding = true;
            }
        }

        let mut control_setup_success = false;

        while !control_setup_success {
            //Tells imu to block register while it's being read
            //Also sets register value to auto-increment if reading
            //multi-register value (CTRL3_c)
            imu.write_reg(0x12, 0x44)?;

            // Disable I2C while preserving other CTRL4_C bits.
            let ctrl4 = imu.read_reg(0x13)?;
            imu.write_reg(0x13, ctrl4 | 0x04)?;

            /*
             * Configures sensors, see datasheet for other configurations
             */

            // 104 Hz, ±2g accelerometer. (CTRL1_XL)
            imu.write_reg(0x10, 0x40)?;

            // 104 Hz, ±250 dps gyroscope. (CTRL2_G)
            imu.write_reg(0x11, 0x40)?;


            /*
             * Checking that configurations worked 
             */
            let mut failed = false;
            for (reg, expected) in [
                (0x10, 0x40), // CTRL1_XL: accelerometer enabled
                (0x11, 0x40), // CTRL2_G: gyroscope enabled
                (0x12, 0x44), // CTRL3_C: BDU + address increment
            ] {
                let actual = imu.read_reg(reg)?;
                eprintln!("Register 0x{reg:02X}: 0x{actual:02X}");
                if actual != expected {
                    eprintln!(
                        "IMU configuration failed: register 0x{reg:02X}, \
                         expected 0x{expected:02X}, got 0x{actual:02X}"
                    );
                    failed = true;
                }
            }
            control_setup_success = !failed;
        }

        Ok(imu)

    }

    
    //Lowkey trusting chat did these read/write functions properly!

    fn read_reg(&mut self, reg: u8) -> Result<u8> {
        let mut buf = [reg | 0x80, 0];
        self.spi.transfer_in_place(&mut buf)?;
        Ok(buf[1])
    }

    fn write_reg(
        &mut self,
        reg: u8,
        value: u8,
    ) -> Result<()> {
        self.spi.write(&[reg & 0x7F, value])?;
        Ok(())
    }

    pub fn read(&mut self) -> Result<ImuData> {
        // Command + 12 data bytes:
        // gyro XYZ, then accel XYZ.
        let mut buf = [0u8; 13];

        // 0x80: Sets read command
        // 0x22: Selects register OUTX_L_G
        buf[0] = 0x22 | 0x80;

        // SPI transfer
        self.spi.transfer_in_place(&mut buf)?;

        // tiny function to pull values from our buffer
        // that are stored in little endian for some weird reason
        // (look up closures : cool thing chat taught me!)
        let raw = |i: usize| -> i16 {
            i16::from_le_bytes([buf[i], buf[i + 1]])
        };

        // 0.061 mg/LSB for ±2g
        // Change this if sensor settings change
        let accel_scale =
            0.061e-3_f64 * 9.80665;

        // 8.75 mdps/LSB for ±250 dps
        // Change this if sensor settings change
        let gyro_scale =
            8.75e-3_f64 *
            std::f64::consts::PI / 180.0;

        Ok(ImuData {
            gyro_x: raw(1) as f64 * gyro_scale,
            gyro_y: raw(3) as f64 * gyro_scale,
            gyro_z: raw(5) as f64 * gyro_scale,

            accel_x: raw(7) as f64 * accel_scale,
            accel_y: raw(9) as f64 * accel_scale,
            accel_z: raw(11) as f64 * accel_scale,
        })
    }

}
