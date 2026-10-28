//! Serial protocol shared with the merceves firmware (elec/src/merceves/main.cpp).
//!
//! Every frame is COBS-encoded and ends in a 0x00 byte. Decoded, a frame is
//! `[type u8][payload][crc u16]`, where the crc is CRC-16/CCITT-FALSE over type + payload.
//! All multi-byte values are little-endian.
//!
//! - jetson -> pico, `MSG_DRIVE`: f32 steering [-1, 1], f32 drivetrain [-1, 1]
//! - pico -> jetson, `MSG_TELEMETRY`: f32 left rpm, f32 right rpm

use crc::{CRC_16_IBM_3740, Crc};

// IBM_3740 is the catalog name for CRC-16/CCITT-FALSE
const CRC16: Crc<u16> = Crc::<u16>::new(&CRC_16_IBM_3740);

pub const MSG_DRIVE: u8 = 0x01;
pub const MSG_TELEMETRY: u8 = 0x02;

/// Encodes a drive command, ready to write to the port (trailing 0x00 included).
pub fn encode_drive(steering: f32, drivetrain: f32) -> Vec<u8> {
    let mut frame = vec![MSG_DRIVE];
    frame.extend_from_slice(&steering.to_le_bytes());
    frame.extend_from_slice(&drivetrain.to_le_bytes());
    frame.extend_from_slice(&CRC16.checksum(&frame).to_le_bytes());

    let mut out = cobs::encode_vec(&frame);
    out.push(0x00);
    out
}

/// Decodes one telemetry frame into (left rpm, right rpm). Pass the bytes before a 0x00, without it.
#[allow(dead_code)] // the firmware doesn't send telemetry yet
pub fn decode_telemetry(encoded: &[u8]) -> Option<(f32, f32)> {
    let frame = cobs::decode_vec(encoded).ok()?;
    if frame.len() != 11 || frame[0] != MSG_TELEMETRY {
        return None;
    }
    let crc = u16::from_le_bytes([frame[9], frame[10]]);
    if CRC16.checksum(&frame[..9]) != crc {
        return None;
    }

    let left = f32::from_le_bytes(frame[1..5].try_into().ok()?);
    let right = f32::from_le_bytes(frame[5..9].try_into().ok()?);
    Some((left, right))
}

#[cfg(test)]
mod tests {
    use super::*;

    // bytes produced by the firmware's own encoder for the same command
    #[test]
    fn drive_matches_firmware() {
        assert_eq!(
            encode_drive(-0.5, 0.25),
            [
                0x02, 0x01, 0x01, 0x01, 0x02, 0xbf, 0x01, 0x05, 0x80, 0x3e, 0x93, 0x74, 0x00
            ]
        );
    }

    #[test]
    fn telemetry_round_trip() {
        let mut frame = vec![MSG_TELEMETRY];
        frame.extend_from_slice(&120.5f32.to_le_bytes());
        frame.extend_from_slice(&(-3.0f32).to_le_bytes());
        frame.extend_from_slice(&CRC16.checksum(&frame).to_le_bytes());
        let encoded = cobs::encode_vec(&frame);

        assert_eq!(decode_telemetry(&encoded), Some((120.5, -3.0)));
    }

    #[test]
    fn telemetry_rejects_bad_crc() {
        let mut frame = vec![MSG_TELEMETRY];
        frame.extend_from_slice(&[0x11; 8]);
        frame.extend_from_slice(&[0x00, 0x00]);
        let encoded = cobs::encode_vec(&frame);

        assert_eq!(decode_telemetry(&encoded), None);
    }
}
