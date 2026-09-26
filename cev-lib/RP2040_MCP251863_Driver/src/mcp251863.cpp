#include "mcp251863.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <utility>

namespace {

std::optional<PayloadSize> canfd_len_to_dlc(size_t len) {
    switch (len) {
        case 0: return PayloadSize::PL_SIZE_MCP_0;
        case 1: return PayloadSize::PL_SIZE_MCP_1;
        case 2: return PayloadSize::PL_SIZE_MCP_2;
        case 3: return PayloadSize::PL_SIZE_MCP_3;
        case 4: return PayloadSize::PL_SIZE_MCP_4;
        case 5: return PayloadSize::PL_SIZE_MCP_5;
        case 6: return PayloadSize::PL_SIZE_MCP_6;
        case 7: return PayloadSize::PL_SIZE_MCP_7;
        case 8: return PayloadSize::PL_SIZE_MCP_8;
        case 12: return PayloadSize::PL_SIZE_MCP_12;
        case 16: return PayloadSize::PL_SIZE_MCP_16;
        case 20: return PayloadSize::PL_SIZE_MCP_20;
        case 24: return PayloadSize::PL_SIZE_MCP_24;
        case 32: return PayloadSize::PL_SIZE_MCP_32;
        case 48: return PayloadSize::PL_SIZE_MCP_48;
        case 64: return PayloadSize::PL_SIZE_MCP_64;
        default: return std::nullopt;
    }
}

int create_message_obj(uint8_t* dst, const CanFdFrame& frame, size_t* objectSize);

uint8_t canfd_dlc_to_len(uint8_t dlc) {
    switch (static_cast<PayloadSize>(dlc & 0x0F)) {
        case PayloadSize::PL_SIZE_MCP_0: return 0;
        case PayloadSize::PL_SIZE_MCP_1: return 1;
        case PayloadSize::PL_SIZE_MCP_2: return 2;
        case PayloadSize::PL_SIZE_MCP_3: return 3;
        case PayloadSize::PL_SIZE_MCP_4: return 4;
        case PayloadSize::PL_SIZE_MCP_5: return 5;
        case PayloadSize::PL_SIZE_MCP_6: return 6;
        case PayloadSize::PL_SIZE_MCP_7: return 7;
        case PayloadSize::PL_SIZE_MCP_8: return 8;
        case PayloadSize::PL_SIZE_MCP_12: return 12;
        case PayloadSize::PL_SIZE_MCP_16: return 16;
        case PayloadSize::PL_SIZE_MCP_20: return 20;
        case PayloadSize::PL_SIZE_MCP_24: return 24;
        case PayloadSize::PL_SIZE_MCP_32: return 32;
        case PayloadSize::PL_SIZE_MCP_48: return 48;
        case PayloadSize::PL_SIZE_MCP_64: return 64;
        default: return 0;
    }
}

uint8_t can_dlc_to_len(uint8_t dlc, bool fdf) {
    if (!fdf && ((dlc & 0x0F) > 8)) {
        return 8;
    }
    return canfd_dlc_to_len(dlc);
}

uint32_t pack_nominal_bit_timing(BitTiming timing) {
    return ((static_cast<uint32_t>(timing.brp) & 0xFF) << 24) |
           ((static_cast<uint32_t>(timing.tseg1) & 0xFF) << 16) |
           ((static_cast<uint32_t>(timing.tseg2) & 0x7F) << 8) |
           (static_cast<uint32_t>(timing.sjw) & 0x7F);
}

uint32_t pack_data_bit_timing(BitTiming timing) {
    return ((static_cast<uint32_t>(timing.brp) & 0xFF) << 24) |
           ((static_cast<uint32_t>(timing.tseg1) & 0xFF) << 16) |
           ((static_cast<uint32_t>(timing.tseg2) & 0x0F) << 8) |
           (static_cast<uint32_t>(timing.sjw) & 0x0F);
}

uint32_t encode_tdc(bool enable, uint8_t offset) {
    if (!enable) {
        return 0;
    }

    // 10 at bits 17:16 for TDCMOD auto
    // TDCO at bits 14:6
    return (2UL << 16) | ((offset & 0x7F) << 8);
}

InitConfig default_init_config() {
    return InitConfig{
        .enablePll         = 0,
        .sclkDiv2          = 0,
        .enableTdc         = 1,
        .rxTimestampEnable = 0,
        .tdcOffset         = 6,
        .txFifo            = 1,
        .rxFifo            = 2,
        // FIFO depth is the number of message slots, 1..32
        // FSIZE register field stores it as 0..31
        // we subtract 1 from fsize at call sites
        .txFifoDepth   = 8,
        .rxFifoDepth   = 8,
        .txPayloadSize = PayloadSize::PL_SIZE_MCP_64,
        .rxPayloadSize = PayloadSize::PL_SIZE_MCP_64,
        // Defaults assume a 40 MHz CAN clock: nominal 500 kbit/s, data 2 Mbit/s.
        .nominalBitTiming = kBitTiming500K40MHz,
        .dataBitTiming    = kBitTiming2M40MHz,
    };
}

uint32_t pack_id_word(const CanFdFrame& frame) {
    if (frame.ide) {
        return ((frame.id >> 18) & 0x7FF) | ((frame.id & 0x3FFFF) << 11);
    }

    const uint32_t sid   = frame.id & 0x7FF;
    const uint32_t sid11 = (frame.sid11 || (frame.id > 0x7FF)) ? ((frame.id >> 11) & 0x01) : 0;
    return sid | (sid11 << 29);
}

uint32_t pack_control_word(const CanFdFrame& frame) {
    return (static_cast<uint32_t>(frame.dlc) & 0x0F) | (frame.ide ? (1UL << 4) : 0) |
           (frame.rtr ? (1UL << 5) : 0) | (frame.brs ? (1UL << 6) : 0) |
           (frame.fdf ? (1UL << 7) : 0) | (frame.esi ? (1UL << 8) : 0);
}

void store_word(uint8_t* dst, uint32_t word) {
    dst[0] = static_cast<uint8_t>(word & 0xFF);
    dst[1] = static_cast<uint8_t>((word >> 8) & 0xFF);
    dst[2] = static_cast<uint8_t>((word >> 16) & 0xFF);
    dst[3] = static_cast<uint8_t>((word >> 24) & 0xFF);
}

// the mcp reads and writes its message ram only in whole 32-bit words
size_t round_up_to_word(size_t len) { return (len + 3) / 4 * 4; }

uint32_t load_word(const uint8_t* src) {
    return (static_cast<uint32_t>(src[0])) | (static_cast<uint32_t>(src[1]) << 8) |
           (static_cast<uint32_t>(src[2]) << 16) | (static_cast<uint32_t>(src[3]) << 24);
}

int finalize_frame_dlc(CanFdFrame* frame) {
    if (frame->fdf) {
        auto dlc_opt = canfd_len_to_dlc(frame->len);
        if (!dlc_opt) {
            return 0;
        }
        frame->dlc = std::to_underlying(*dlc_opt);
        return 1;
    }

    if (frame->len > 8) {
        return 0;
    }
    frame->dlc = frame->len;
    frame->brs = false;
    return 1;
}

int validate_tx_frame(const CanFdFrame& frame) {
    if (frame.ide && (frame.id > 0x1FFFFFFF)) {
        return 0;
    }
    if (!frame.ide && (frame.id > 0x7FF)) {
        return 0;
    }
    if (!frame.fdf && frame.brs) {
        return 0;
    }
    if (frame.len > 64) {
        return 0;
    }
    return 1;
}

CanFdFrame decode_rx_header(const uint8_t* header, bool timestampEnabled) {
    const uint32_t word0 = load_word(header);
    const uint32_t word1 = load_word(header + 4);
    const uint8_t dlc    = word1 & 0x0F;
    const bool ide       = (word1 & (1UL << 4)) != 0;
    const bool fdf       = (word1 & (1UL << 7)) != 0;
    const bool sid11     = (word0 & (1UL << 29)) != 0;

    return CanFdFrame{
        .id              = ide ? ((word0 & 0x7FF) << 18) | ((word0 >> 11) & 0x3FFFF)
                               : (word0 & 0x7FF) | (static_cast<uint32_t>(sid11) << 11),
        .dlc             = dlc,
        .len             = can_dlc_to_len(dlc, fdf),
        .ide             = ide,
        .fdf             = fdf,
        .brs             = (word1 & (1UL << 6)) != 0,
        .rtr             = (word1 & (1UL << 5)) != 0,
        .esi             = (word1 & (1UL << 8)) != 0,
        .sid11           = sid11,
        .filter_hit      = static_cast<uint8_t>((word1 >> 11) & 0x1F),
        .timestamp_valid = timestampEnabled,
        .timestamp       = timestampEnabled ? load_word(header + 8) : 0,
    };
}

int create_message_obj(uint8_t* dst, const CanFdFrame& frame, size_t* objectSize) {
    CanFdFrame txFrame = frame;
    if (!validate_tx_frame(txFrame)) {
        return 0;
    }
    if (!finalize_frame_dlc(&txFrame)) {
        return 0;
    }
    const size_t size = 8 + round_up_to_word(txFrame.len);
    memset(dst, 0, size);
    store_word(dst, pack_id_word(txFrame));
    store_word(dst + 4, pack_control_word(txFrame));
    std::copy_n(txFrame.data, txFrame.len, dst + 8);
    if (objectSize != nullptr) {
        *objectSize = size;
    }
    return 1;
}

}  // namespace

MCP251863::MCP251863(spi_inst_t* ispi, uint iCSPin, uint iSTBYPin)
    : spi_(ispi), chipSelectPin_(iCSPin), standbyPin_(iSTBYPin) {}

int MCP251863::writeAddr(uint16_t startAddr, const uint8_t* data, size_t len) {
    Command cmd{};
    switch (writeMode_) {
        case WriteMode::WM_MCP_NORM: cmd = Command::CMD_MCP_WRITA; break;
        case WriteMode::WM_MCP_CRC: cmd = Command::CMD_MCP_WRACR; break;
        case WriteMode::WM_MCP_SAFE: cmd = Command::CMD_MCP_WRASF; break;
        default: return 0;
    }
    // form message CCCC-AAAAAAAAAAAA
    uint8_t message[2];

    message[0] = (std::to_underlying(cmd) << 4) | (startAddr >> 8);
    message[1] = (startAddr << 4) >> 4;

    // drive CS pin low
    asm volatile("nop \n nop \n nop");
    gpio_put(chipSelectPin_, false);
    asm volatile("nop \n nop \n nop");

    // transmit message via SPI
    spi_write_blocking(spi_, message, 2);

    // write data
    spi_write_blocking(spi_, data, len);

    // drive CS pin high, ending read cycle
    asm volatile("nop \n nop \n nop");
    gpio_put(chipSelectPin_, true);
    asm volatile("nop \n nop \n nop");

    return 1;
}

int MCP251863::readAddr(uint16_t startAddr, uint8_t* dst, size_t len) {
    // set read command based on read mode
    Command cmd{};
    uint8_t message[3];
    switch (readMode_) {
        case ReadMode::RM_MCP_NORM: cmd = Command::CMD_MCP_READA; break;
        case ReadMode::RM_MCP_CRC: cmd = Command::CMD_MCP_RDACR; break;
        default: return 0;
    }
    // form message CCCC-AAAAAAAAAAAA
    message[0] = (std::to_underlying(cmd) << 4) | (startAddr >> 8);
    message[1] = (startAddr << 4) >> 4;

    // if (readMode == rm_MCP251863_t::READ_CRC) {
    //     message[2] = (uint8_t)len;
    // }

    // drive CS pin low
    asm volatile("nop \n nop \n nop");
    gpio_put(chipSelectPin_, false);
    asm volatile("nop \n nop \n nop");

    // transmit message via SPI
    spi_write_blocking(spi_, message, 2);

    // write extra bits for len if CRC mode
    // if (readMode == rm_MCP251863_t::READ_CRC) {
    //    spi_write_blocking(spi, (message)+2, 1);
    //}

    // read out data
    spi_read_blocking(spi_, 0, dst, len);

    // read out CRC
    // if (readMode == rm_MCP251863_t::READ_CRC) {
    //    spi_read16_blocking(spi, 0, &crc, 1);
    //}

    // drive CS pin high, ending read cycle
    asm volatile("nop \n nop \n nop");
    gpio_put(chipSelectPin_, true);
    asm volatile("nop \n nop \n nop");

    // check CRC
    // later

    return 1;
}

uint32_t MCP251863::readReg32(uint16_t addr) {
    uint8_t buff[4]{};
    readAddr(addr, buff, 4);
    return load_word(buff);
}

int MCP251863::writeReg32(uint16_t addr, uint32_t value) {
    uint8_t buff[4]{};
    store_word(buff, value);
    return writeAddr(addr, buff, 4);
}

int MCP251863::waitForByte(uint16_t addr, uint8_t mask, uint8_t value) {
    for (int i = 0; i < 100; i++) {
        uint8_t buff{};
        readAddr(addr, &buff, 1);
        if ((buff & mask) == value) {
            return 1;
        }
        sleep_ms(1);
    }
    return 0;
}

int MCP251863::init() { return init(default_init_config()); }

int MCP251863::init(const InitConfig& config) {
    // txFifo, rxFifo, txFifoDepth, rxFifoDepth must be 1..32 inclusive
    if ((config.txFifo < 1) || (config.txFifo > 31) || (config.rxFifo < 1) ||
        (config.rxFifo > 31) || (config.txFifo == config.rxFifo) || (config.txFifoDepth < 1) ||
        (config.txFifoDepth > 32) || (config.rxFifoDepth < 1) || (config.rxFifoDepth > 32)) {
        return 0;
    }

    uint8_t one        = 1;
    const uint8_t zero = 0;

    gpio_init(chipSelectPin_);
    gpio_set_dir(chipSelectPin_, GPIO_OUT);
    gpio_put(chipSelectPin_, true);
    gpio_init(standbyPin_);
    gpio_set_dir(standbyPin_, GPIO_OUT);
    setTransceiverMode(TransceiverMode::TMODE_MCP_STBY);

    writeMode_ = WriteMode::WM_MCP_NORM;
    readMode_  = ReadMode::RM_MCP_NORM;

    if (!reset()) {
        return 0;
    }
    sleep_ms(10);

    // Wait for oscillator stability before touching CAN timing.
    if (!waitForByte(std::to_underlying(RegisterAddress::REG_MCP_OSC) + 1, 1 << 2, 1 << 2)) {
        return 0;
    }

    if (!setControllerMode(ControllerMode::CMODE_MCP_CONF)) {
        return 0;
    }

    const uint8_t osc = (config.enablePll ? 0x01 : 0x00) | (config.sclkDiv2 ? 0x10 : 0x00);
    writeAddr(std::to_underlying(RegisterAddress::REG_MCP_OSC), &osc, 1);
    if (config.enablePll &&
        !waitForByte(std::to_underlying(RegisterAddress::REG_MCP_OSC) + 1, 0x01, 0x01)) {
        return 0;
    }
    if (config.sclkDiv2 &&
        !waitForByte(std::to_underlying(RegisterAddress::REG_MCP_OSC) + 1, 1 << 4, 1 << 4)) {
        return 0;
    }

    if (!setBitTiming(config.nominalBitTiming, config.dataBitTiming)) {
        return 0;
    }

    writeReg32(std::to_underlying(RegisterAddress::REG_MCP_C1TDC),
               encode_tdc(config.enableTdc, config.tdcOffset));

    txFifoNum_           = config.txFifo;
    rxFifoNum_           = config.rxFifo;
    rxTimestampsEnabled_ = config.rxTimestampEnable != 0;

    FifoInterruptFlag txFlags[] = {FifoInterruptFlag::FIFO_INT_MCP_NFNE,
                                   FifoInterruptFlag::FIFO_INT_MCO_TXAT};
    FifoInterruptFlag rxFlags[] = {FifoInterruptFlag::FIFO_INT_MCP_NFNE,
                                   FifoInterruptFlag::FIFO_INT_MCP_OVFL};
    if (!initGeneralPurposeFifo(txFifoNum_, FifoMode::FIFO_MODE_MCP_TX, config.txPayloadSize,
                                config.txFifoDepth, 1, TxRetransmitMode::TXRET_MCP_UNLIM, txFlags,
                                2)) {
        return 0;
    }
    if (!initGeneralPurposeFifo(rxFifoNum_, FifoMode::FIFO_MODE_MCP_RX, config.rxPayloadSize,
                                config.rxFifoDepth, 0, TxRetransmitMode::TXRET_MCP_NONE, rxFlags,
                                2)) {
        return 0;
    }
    if (rxTimestampsEnabled_) {
        const uint16_t rx_fifo_addr =
            std::to_underlying(RegisterAddress::REG_MCP_C1FIFOCONx) + 12 * (rxFifoNum_ - 1);
        readAddr(rx_fifo_addr, &one, 1);
        one |= (1 << 5);
        writeAddr(rx_fifo_addr, &one, 1);
    }

    // Disable filter 0 while programming it, then make it accept all frames into rxFifoNum.
    const uint16_t flt_ctrl_addr = std::to_underlying(RegisterAddress::REG_MCP_C1FLTCONx);
    writeAddr(flt_ctrl_addr, &zero, 1);
    writeReg32(std::to_underlying(RegisterAddress::REG_MCP_C1FLTOBJx), 0);
    writeReg32(std::to_underlying(RegisterAddress::REG_MCP_C1MASKx), 0);
    one = 0b10000000 | (rxFifoNum_ & 0b00011111);
    writeAddr(flt_ctrl_addr, &one, 1);

    writeReg32(std::to_underlying(RegisterAddress::REG_MCP_C1RXIF), 0xFFFFFFFF);
    writeReg32(std::to_underlying(RegisterAddress::REG_MCP_C1TXIF), 0xFFFFFFFF);
    writeReg32(std::to_underlying(RegisterAddress::REG_MCP_C1RXOVIF), 0xFFFFFFFF);
    writeReg32(std::to_underlying(RegisterAddress::REG_MCP_C1TXATIF), 0xFFFFFFFF);

    InterruptEnable interrupts[] = {InterruptEnable::INT_EN_MCP_RXIE,
                                    InterruptEnable::INT_EN_MCP_RXOVIE};
    if (!setInterrupts(interrupts, sizeof(interrupts) / sizeof(interrupts[0]))) {
        return 0;
    }

    setTransceiverMode(TransceiverMode::TMODE_MCP_NORM);
    if (!setControllerMode(ControllerMode::CMODE_MCP_CFD_NORM)) {
        return 0;
    }

    return waitForByte(std::to_underlying(RegisterAddress::REG_MCP_C1CON) + 2, 0b11100000,
                       std::to_underlying(ControllerMode::CMODE_MCP_CFD_NORM) << 5);
}

int MCP251863::setBitTiming(BitTiming nominalTiming, BitTiming dataTiming) {
    writeReg32(std::to_underlying(RegisterAddress::REG_MCP_C1NBTCFG),
               pack_nominal_bit_timing(nominalTiming));
    writeReg32(std::to_underlying(RegisterAddress::REG_MCP_C1DBTCFG),
               pack_data_bit_timing(dataTiming));

    return 1;
}

int MCP251863::reset() {
    const Command cmd = Command::CMD_MCP_RESET;
    uint8_t message[2]{};

    message[0] = std::to_underlying(cmd) << 4;

    // drive CS pin low
    asm volatile("nop \n nop \n nop");
    gpio_put(chipSelectPin_, false);
    asm volatile("nop \n nop \n nop");

    // transmit message via SPI
    spi_write_blocking(spi_, message, 2);

    // drive CS pin high, ending read cycle
    asm volatile("nop \n nop \n nop");
    gpio_put(chipSelectPin_, true);
    asm volatile("nop \n nop \n nop");

    return 1;
}

int MCP251863::initGeneralPurposeFifo(uint8_t fifoNum, FifoMode fifoMode, PayloadSize plSize,
                                      uint8_t fSize, uint8_t prioNum, TxRetransmitMode retranMode,
                                      const FifoInterruptFlag* intFlagArray, size_t intFlagSize) {
    uint8_t buff[4];
    const uint16_t addr =
        std::to_underlying(RegisterAddress::REG_MCP_C1FIFOCONx) + 12 * (fifoNum - 1);

    uint8_t intFlags = 0;
    for (size_t i = 0; i < intFlagSize; i++) {
        intFlags |= static_cast<uint8_t>(intFlagArray[i]);
    }

    buff[0] = intFlags | (std::to_underlying(fifoMode) << 7);
    buff[1] = 0b00000000;
    // assumes prioNum <= 32
    buff[2] = 0b00000000 | (std::to_underlying(retranMode) << 5) | prioNum;
    // FSIZE stores depth-1 (ie 0 = 1 message; 31 = 32 messages), but the caller passes 1..32
    buff[3] = ((std::to_underlying(plSize) & 0b111) << 5) | ((fSize - 1) & 0x1F);

    writeAddr(addr, buff, 4);
    return 1;
}

int MCP251863::initTransmitEventFifo(uint8_t fSize, const FifoInterruptFlag* intFlagArray,
                                     size_t intFlagSize) {
    uint8_t buff[4];
    const uint16_t addr = std::to_underlying(RegisterAddress::REG_MCP_C1TEFCON);

    uint8_t intFlags = 0;
    for (size_t i = 0; i < intFlagSize; i++) {
        intFlags |= static_cast<uint8_t>(intFlagArray[i]);
    }

    // wait for reset bit to clear
    if (!waitForByte(addr + 1, 1 << 2, 0)) {
        return 0;
    }

    // set bytes
    buff[0] = 0b00000000 | intFlags;
    buff[1] = 0b00000000;
    buff[2] = 0b00000000;
    // FSIZE stores depth-1 (ie 0 = 1 message; 31 = 32 messages), but the caller passes 1..32
    buff[3] = (fSize - 1) & 0x1F;

    writeAddr(addr, buff, 4);
    return 1;
}

int MCP251863::initTransmitQueue(PayloadSize plSize, uint8_t fSize, uint8_t prioNum,
                                 TxRetransmitMode retranMode, const FifoInterruptFlag* intFlagArray,
                                 size_t intFlagSize) {
    uint8_t buff[4];
    const uint16_t addr = std::to_underlying(RegisterAddress::REG_MCP_C1TXQCON);

    uint8_t intFlags = 0;
    for (size_t i = 0; i < intFlagSize; i++) {
        intFlags |= static_cast<uint8_t>(intFlagArray[i]);
    }

    // wait for reset bit to clear
    if (!waitForByte(addr + 1, 1 << 2, 0)) {
        return 0;
    }

    // set bytes
    buff[0] = 0b00000000 | intFlags;
    buff[1] = 0b00000000;
    // assumes prioNum <= 32
    buff[2] = 0b00000000 | (std::to_underlying(retranMode) << 5) | prioNum;
    // FSIZE stores depth-1 (ie 0 = 1 message; 31 = 32 messages), but the caller passes 1..32
    buff[3] = ((std::to_underlying(plSize) & 0b111) << 5) | ((fSize - 1) & 0x1F);

    writeAddr(addr, buff, 4);
    return 1;
}

int MCP251863::initFilter(uint8_t fltNum, uint8_t fifoNum, uint16_t canSID) {
    // C1FLTCONm holds four bytes, packed into 32 a 32 bit register
    // the offset is REG_MCP_C1FLTCONx + 4*(N/4), and byte offset is N%4
    // this simplifies is just REG_MCP_C1FLTCONx + N
    const uint16_t flt_addr = std::to_underlying(RegisterAddress::REG_MCP_C1FLTCONx) + fltNum;
    const uint16_t flt_obj_addr =
        std::to_underlying(RegisterAddress::REG_MCP_C1FLTOBJx) + 8 * fltNum;

    uint8_t buff[4];

    // enable filter, assumes fifoNum <= 32
    buff[0] = 0b00000000 | fifoNum | (1 << 7);
    writeAddr(flt_addr, buff, 1);

    // Pack SID[10:0] (and SID11 in bit 11) into C1FLTOBJn bits [11:0]
    buff[0] = canSID & 0xFF;
    buff[1] = (canSID >> 8) & 0x0F;
    buff[2] = 0x00;
    buff[3] = 0x00;

    writeAddr(flt_obj_addr, buff, 4);

    return 1;
}

int MCP251863::pushTXFIFO(uint8_t fifoNum, const uint8_t* data, size_t pSize) {
    const uint16_t fifo_addr =
        std::to_underlying(RegisterAddress::REG_MCP_C1FIFOCONx) + 12 * (fifoNum - 1);
    const uint16_t fifo_stat_addr =
        std::to_underlying(RegisterAddress::REG_MCP_C1FIFOSTAx) + 12 * (fifoNum - 1);
    const uint16_t fifo_point_addr =
        std::to_underlying(RegisterAddress::REG_MCP_C1FIFOUAx) + 12 * (fifoNum - 1);

    uint8_t buff{};
    uint16_t message_addr{};

    // check if fifo nonfull, return if it is. We will implement real error handling later
    readAddr(fifo_stat_addr, &buff, 1);
    if ((buff & 0b00000001) == 0) {
        return 0;
    }

    // get pointer to message object from FIFO, the addresses are only 12-bits wide?
    message_addr = static_cast<uint16_t>(readReg32(fifo_point_addr));
    message_addr += 0x400;

    // write message to addr
    writeAddr(message_addr, data, pSize);

    // UINC queues the frame and TXREQ asks the chip to send it, in one write
    buff = 0b00000011;
    writeAddr(fifo_addr + 1, &buff, 1);

    return 1;
}

int MCP251863::popRXFIFO(uint8_t fifoNum, uint8_t* dst, size_t pSize) {
    const uint16_t fifo_addr =
        std::to_underlying(RegisterAddress::REG_MCP_C1FIFOCONx) + 12 * (fifoNum - 1);
    const uint16_t fifo_stat_addr =
        std::to_underlying(RegisterAddress::REG_MCP_C1FIFOSTAx) + 12 * (fifoNum - 1);
    const uint16_t fifo_point_addr =
        std::to_underlying(RegisterAddress::REG_MCP_C1FIFOUAx) + 12 * (fifoNum - 1);

    uint8_t buff{};
    uint16_t message_addr{};

    // check if fifo nonempty, if it is return
    readAddr(fifo_stat_addr, &buff, 1);
    if ((buff & 0b00000001) == 0) {
        return 0;
    }

    message_addr = static_cast<uint16_t>(readReg32(fifo_point_addr));
    message_addr += 0x400;

    // read in message
    readAddr(message_addr, dst, pSize);

    // decrement fifo
    buff = 0b00000001;
    writeAddr(fifo_addr + 1, &buff, 1);

    return 1;
}

int MCP251863::send_canfd(uint32_t id, const uint8_t* data, size_t len, bool brs,
                          bool extended_id) {
    return send_canfd(txFifoNum_, id, data, len, brs, extended_id);
}

int MCP251863::send_canfd(uint8_t fifoNum, uint32_t id, const uint8_t* data, size_t len, bool brs,
                          bool extended_id) {
    auto dlc_opt = canfd_len_to_dlc(len);
    if (!dlc_opt) {
        return 0;
    }
    if ((len > 0) && (data == nullptr)) {
        return 0;
    }

    CanFdFrame frame{
        .id  = id,
        .dlc = std::to_underlying(*dlc_opt),
        .len = static_cast<uint8_t>(len),
        .ide = extended_id,
        .fdf = true,
        .brs = brs,
    };
    std::copy_n(data, len, frame.data);
    return send_frame(fifoNum, frame);
}

int MCP251863::send_frame(const CanFdFrame& frame) { return send_frame(txFifoNum_, frame); }

int MCP251863::send_frame(uint8_t fifoNum, const CanFdFrame& frame) {
    uint8_t message[72];
    size_t objectSize = 0;
    if (!create_message_obj(message, frame, &objectSize)) {
        return 0;
    }
    return pushTXFIFO(fifoNum, message, objectSize);
}

CanFdFrame MCP251863::read_canfd() { return read_frame(rxFifoNum_); }

CanFdFrame MCP251863::read_canfd(uint8_t fifoNum) { return read_frame(fifoNum); }

CanFdFrame MCP251863::read_frame() { return read_frame(rxFifoNum_); }

CanFdFrame MCP251863::read_frame(uint8_t fifoNum) {
    CanFdFrame frame{};

    const uint16_t fifo_addr =
        std::to_underlying(RegisterAddress::REG_MCP_C1FIFOCONx) + 12 * (fifoNum - 1);
    const uint16_t fifo_stat_addr =
        std::to_underlying(RegisterAddress::REG_MCP_C1FIFOSTAx) + 12 * (fifoNum - 1);
    const uint16_t fifo_point_addr =
        std::to_underlying(RegisterAddress::REG_MCP_C1FIFOUAx) + 12 * (fifoNum - 1);

    uint8_t buff{};
    uint16_t message_addr{};
    uint8_t header[12]{};
    const size_t headerSize = rxTimestampsEnabled_ ? 12 : 8;

    readAddr(fifo_stat_addr, &buff, 1);
    if ((buff & 0b00000001) == 0) {
        return frame;
    }

    message_addr = static_cast<uint16_t>(readReg32(fifo_point_addr));
    message_addr += 0x400;
    readAddr(message_addr, header, headerSize);
    frame = decode_rx_header(header, rxTimestampsEnabled_);

    if (frame.len > 0) {
        readAddr(message_addr + headerSize, frame.data, round_up_to_word(frame.len));
    }

    buff = 0b00000001;
    writeAddr(fifo_addr + 1, &buff, 1);

    frame.valid = true;
    return frame;
}

int MCP251863::clearRxOverflow() {
    // writing zero clears RXOVIF, the read-only flags in this byte ignore the write
    const uint8_t zero = 0;
    writeAddr(std::to_underlying(RegisterAddress::REG_MCP_C1FIFOSTAx) + 12 * (rxFifoNum_ - 1),
              &zero, 1);
    return 1;
}

FifoStatus MCP251863::getFIFOStatus(uint8_t fifoNum) {
    const uint16_t fifo_stat_addr =
        std::to_underlying(RegisterAddress::REG_MCP_C1FIFOSTAx) + 12 * (fifoNum - 1);
    const uint32_t reg = readReg32(fifo_stat_addr);

    return FifoStatus{
        .fifo_index              = static_cast<uint8_t>((reg >> 8) & 0x1F),
        .tx_aborted              = (reg & (1UL << 7)) != 0,
        .tx_lost_arbitration     = (reg & (1UL << 6)) != 0,
        .tx_error                = (reg & (1UL << 5)) != 0,
        .tx_attempts_exhausted   = (reg & (1UL << 4)) != 0,
        .rx_overflow             = (reg & (1UL << 3)) != 0,
        .empty_or_full           = (reg & (1UL << 2)) != 0,
        .half_empty_or_half_full = (reg & (1UL << 1)) != 0,
        .not_full_or_not_empty   = (reg & 1UL) != 0,
    };
}

Status MCP251863::getStatus() {
    const uint32_t trec = readReg32(std::to_underlying(RegisterAddress::REG_MCP_C1TREC));
    const uint32_t crc  = readReg32(std::to_underlying(RegisterAddress::REG_MCP_CRC));

    return Status{
        .interrupt_flags  = readReg32(std::to_underlying(RegisterAddress::REG_MCP_C1INT)),
        .rx_if            = readReg32(std::to_underlying(RegisterAddress::REG_MCP_C1RXIF)),
        .tx_if            = readReg32(std::to_underlying(RegisterAddress::REG_MCP_C1TXIF)),
        .rx_overflow_if   = readReg32(std::to_underlying(RegisterAddress::REG_MCP_C1RXOVIF)),
        .tx_attempt_if    = readReg32(std::to_underlying(RegisterAddress::REG_MCP_C1TXATIF)),
        .trec             = trec,
        .bdiag0           = readReg32(std::to_underlying(RegisterAddress::REG_MCP_C1BDIAGx)),
        .bdiag1           = readReg32(std::to_underlying(RegisterAddress::REG_MCP_C1BDIAGx) + 4),
        .crc              = crc,
        .bus_off          = (trec & (1UL << 21)) != 0,
        .tx_error_passive = (trec & (1UL << 20)) != 0,
        .rx_error_passive = (trec & (1UL << 19)) != 0,
        .tx_error_warning = (trec & (1UL << 18)) != 0,
        .rx_error_warning = (trec & (1UL << 17)) != 0,
        .error_warning    = (trec & (1UL << 16)) != 0,
        .tx_error_count   = static_cast<uint8_t>((trec >> 8) & 0xFF),
        .rx_error_count   = static_cast<uint8_t>(trec & 0xFF),
        .spi_crc_format_error = (crc & (1UL << 17)) != 0,
        .spi_crc_error        = (crc & (1UL << 16)) != 0,
    };
}

int MCP251863::setControllerMode(ControllerMode contMode) {
    const uint16_t addr = std::to_underlying(RegisterAddress::REG_MCP_C1CON);
    uint8_t buff0{};
    uint8_t buff1{};

    // read current contMode
    readAddr(addr + 2, &buff0, 1);

    if ((buff0 >> 5) != std::to_underlying(ControllerMode::CMODE_MCP_CONF)) {
        readAddr(addr + 3, &buff1, 1);
        buff1 = (buff1 & 0b11111000) | std::to_underlying(ControllerMode::CMODE_MCP_CONF);
        writeAddr(addr + 3, &buff1, 1);

        if (!waitForByte(addr + 2, 0b11100000,
                         std::to_underlying(ControllerMode::CMODE_MCP_CONF) << 5)) {
            return 0;
        }
    }

    // write intended contMode
    readAddr(addr + 3, &buff1, 1);
    buff1 = (buff1 & 0b11111000) | std::to_underlying(contMode);
    writeAddr(addr + 3, &buff1, 1);

    return 1;
}

int MCP251863::setTransceiverMode(TransceiverMode mode) {
    gpio_put(standbyPin_, std::to_underlying(mode));
    return 1;
}

int MCP251863::setInterrupts(const InterruptEnable* intEnArray, size_t intEnSize) {
    // illegal size
    if (intEnSize > 32) {
        return 0;
    }
    uint32_t message = 0;
    for (size_t i = 0; i < intEnSize; i++) {
        message |= static_cast<uint32_t>(intEnArray[i]);
    }
    writeReg32(std::to_underlying(RegisterAddress::REG_MCP_C1INT), message);
    return 1;
}

int MCP251863::setPinMode(IoPin pin, IoMode mode) {
    uint8_t buff[4]{};
    readAddr(std::to_underlying(RegisterAddress::REG_MCP_IOCON), buff, 4);
    if (pin == IoPin::IO_MCP_INT0) {
        switch (mode) {
            case IoMode::IOMODE_MCP_GPIO_IN:
                buff[0] |= 0b00000001;
                buff[3] |= 0b00000001;
                break;
            case IoMode::IOMODE_MCP_GPIO_OUT:
                buff[0] &= 0b11111110;
                buff[3] |= 0b00000001;
                break;
            case IoMode::IOMODE_MCP_INT: buff[3] &= 0b11111110; break;
            default: return 0;
        }
    } else if (pin == IoPin::IO_MCP_INT1) {
        switch (mode) {
            case IoMode::IOMODE_MCP_GPIO_IN:
                buff[0] |= 0b00000010;
                buff[3] |= 0b00000010;
                break;
            case IoMode::IOMODE_MCP_GPIO_OUT:
                buff[0] &= 0b11111101;
                buff[3] |= 0b00000010;
                break;
            case IoMode::IOMODE_MCP_INT: buff[3] &= 0b11111101; break;
            default: return 0;
        }
    } else {
        return 0;
    }
    writeAddr(std::to_underlying(RegisterAddress::REG_MCP_IOCON), buff, 4);
    return 1;
}

// C1VEC layout
// 31:24 rxcode (third byte)
// 23:16 txcode (second byte)
// 12:8 filhit (first byte, + some offset stuff)
// 7:0 icode (zeroth byte)
// 0x7F = no interrupt, so we & with it

int MCP251863::getTXCode() {
    uint8_t buff{};
    readAddr(std::to_underlying(RegisterAddress::REG_MCP_C1VEC) + 2, &buff, 1);
    return buff & 0x7F;
}

int MCP251863::getRXCode() {
    uint8_t buff{};
    readAddr(std::to_underlying(RegisterAddress::REG_MCP_C1VEC) + 3, &buff, 1);
    return buff & 0x7F;
}

int MCP251863::getFLTCode() {
    // its 5 bits so we mask with 0x1F instead of 0x7F
    uint8_t buff{};
    readAddr(std::to_underlying(RegisterAddress::REG_MCP_C1VEC) + 1, &buff, 1);
    return buff & 0x1F;
}

int MCP251863::getICode() {
    uint8_t buff{};
    readAddr(std::to_underlying(RegisterAddress::REG_MCP_C1VEC), &buff, 1);
    buff &= 0x7F;
    if (buff > 0b1001010) {  // this is the max valid ICODE value
        return -1;
    }
    return buff;
}
