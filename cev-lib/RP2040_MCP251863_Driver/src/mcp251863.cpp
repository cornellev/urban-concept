#include "mcp251863.hpp"

#include <optional>
#include <type_traits>

// implementation of C++23 std::to_underlying
template <typename T>
constexpr auto to_underlying(T value) -> std::underlying_type_t<T> {
    return static_cast<typename std::underlying_type<T>::type>(value);
}

#define MCP_TRY(expr) 
    do {
        const Error e = (expr);
        if (e != Error::None) {
            return e;
        }
    } while (0)

MCP251863* MCP251863::instance_ = nullptr;

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

static uint8_t canfd_dlc_to_len(uint8_t dlc) {
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

uint8_t fifo_plsize_to_len(FifoPayloadSize size) {
    switch (size) {
        case FifoPayloadSize::FIFO_PLSIZE_8: return 8;
        case FifoPayloadSize::FIFO_PLSIZE_12: return 12;
        case FifoPayloadSize::FIFO_PLSIZE_16: return 16;
        case FifoPayloadSize::FIFO_PLSIZE_20: return 20;
        case FifoPayloadSize::FIFO_PLSIZE_24: return 24;
        case FifoPayloadSize::FIFO_PLSIZE_32: return 32;
        case FifoPayloadSize::FIFO_PLSIZE_48: return 48;
        case FifoPayloadSize::FIFO_PLSIZE_64: return 64;
        default: return 0;
    }
}

uint8_t can_dlc_to_len(uint8_t dlc, bool fdf) {
    if (!fdf && ((dlc & 0x0F) > 8)) {
        return 8; 
    }
    return canfd_dlc_to_len(dlc);
}

int create_message_obj(uint8_t* dst, const CanFdFrame& frame, size_t* objectSize);

uint32_t pack_nominal_bit_timing(BitTiming timing) {
    return (((uint32_t)timing.brp & 0xFF) << 24) |
           (((uint32_t)timing.tseg1 & 0xFF) << 16) |
           (((uint32_t)timing.tseg2 & 0x7F) << 8) |
           ((uint32_t)timing.sjw & 0x7F);
}

uint32_t pack_data_bit_timing(BitTiming timing) {
    return (((uint32_t)timing.brp & 0xFF) << 24) |
           (((uint32_t)timing.tseg1 & 0xFF) << 16) |
           (((uint32_t)timing.tseg2 & 0x0F) << 8) |
           ((uint32_t)timing.sjw & 0x0F);
}

Error validate_bit_timing(const BitTiming& nominal, const BitTiming& data) {
    if (nominal.tseg2 > 0x7F || nominal.sjw > 0x7F) {
        return Error::InvalidBitTiming;
    }
    if (data.tseg1 > 0x1F || data.tseg2 > 0x0F || data.sjw > 0x0F) {
        return Error::InvalidBitTiming;
    }
    return Error::None;
}

uint32_t encode_tdc(bool enable, uint8_t offset) {
    if (!enable) {
        return 0;
    }

    // 10 at bits 17:16 for TDCMOD auto
    // TDCO at bits 14:6
    return (2UL << 16) | ((offset & 0x3F) << 8);
}

InitConfig default_init_config() {
    InitConfig config{};

    config.enablePll = 0;
    config.sclkDiv2 = 0;
    config.enableTdc = 1;
    config.rxTimestampEnable = 0;
    config.tdcOffset = 6;
    config.txFifo = 1;
    config.rxFifo = 2;

    // FIFO depth is the number of message slots, 1..32
    // FSIZE register field stores it as 0..31
    // we subtract 1 from fsize at call sites
    config.txFifoDepth = 8;
    config.rxFifoDepth = 8;

    config.txPayloadSize     = PayloadSize::PL_SIZE_MCP_64;
    config.rxPayloadSize     = PayloadSize::PL_SIZE_MCP_64;

    // Defaults assume a 40 MHz CAN clock: nominal 500 kbit/s, data 2 Mbit/s.
    config.nominalBitTiming = kBitTiming500K40MHz;
    config.dataBitTiming    = kBitTiming2M40MHz;

    return config;
}

uint32_t pack_id_word(const CanFdFrame& frame) {
    if (frame.ide) {
        return ((frame.id >> 18) & 0x7FF) | ((frame.id & 0x3FFFF) << 11);
    }

    uint32_t sid = frame.id & 0x7FF;
    uint32_t sid11 = (frame.sid11 || (frame.id > 0x7FF)) ? ((frame.id >> 11) & 0x01) : 0;
    return sid | (sid11 << 29);
}

uint32_t pack_control_word(const CanFdFrame& frame) {
    return ((uint32_t)frame.dlc & 0x0F) |
           (frame.ide ? (1UL << 4) : 0) |
           (frame.rtr ? (1UL << 5) : 0) |
           (frame.brs ? (1UL << 6) : 0) |
           (frame.fdf ? (1UL << 7) : 0) |
           (frame.esi ? (1UL << 8) : 0);
}

void store_word(uint8_t* dst, uint32_t word) {
    dst[0] = (uint8_t)(word & 0xFF);
    dst[1] = (uint8_t)((word >> 8) & 0xFF);
    dst[2] = (uint8_t)((word >> 16) & 0xFF);
    dst[3] = (uint8_t)((word >> 24) & 0xFF);
}

uint32_t load_word(const uint8_t* src) {
    return ((uint32_t)src[0]) |
        ((uint32_t)src[1] << 8) |
        ((uint32_t)src[2] << 16) |
        ((uint32_t)src[3] << 24);
}

Error finalize_frame_dlc(CanFdFrame* frame) {
    if (frame->fdf) {
        auto dlc_opt = canfd_len_to_dlc(frame->len);
        if (!dlc_opt) {
            return Error::InvalidPayloadLength;
        }
        frame->dlc = to_underlying(*dlc_opt);
        return Error::None;
    }

    if (frame->len > 8) {
        return Error::PayloadTooLong;
    }
    frame->dlc = frame->len;
    frame->brs = 0;
    return Error::None;
}

Error validate_tx_frame(const CanFdFrame& frame) {
    if (frame.ide && (frame.id > 0x1FFFFFFF)) {
        return Error::IdOutOfRange;
    }
    if (!frame.ide && (frame.id > 0x7FF)) {
        return Error::IdOutOfRange;
    }
    if (!frame.fdf && frame.brs) {
        return Error::InvalidFlagCombination;
    }
    if (frame.len > 64) {
        return Error::PayloadTooLong;
    }
    if (!frame.fdf && frame.len > 8) {
        return Error::PayloadTooLong;
    }
    return Error::None;
}

CanFdFrame decode_rx_header(const uint8_t* header, bool timestampEnabled) {
    CanFdFrame frame{};

    uint32_t word0 = load_word(header);
    uint32_t word1 = load_word(header + 4);
    frame.dlc = word1 & 0x0F;
    frame.ide = (word1 & (1UL << 4)) != 0;
    frame.rtr = (word1 & (1UL << 5)) != 0;
    frame.brs = (word1 & (1UL << 6)) != 0;
    frame.fdf = (word1 & (1UL << 7)) != 0;
    frame.esi = (word1 & (1UL << 8)) != 0;
    frame.filter_hit = (word1 >> 11) & 0x1F;
    frame.sid11 = (word0 & (1UL << 29)) != 0;
    frame.len = can_dlc_to_len(frame.dlc, frame.fdf);
    frame.timestamp_valid = timestampEnabled;
    if (timestampEnabled) {
        frame.timestamp = load_word(header + 8);
    }
    // suspicious
    if (frame.ide) {
        frame.id = ((word0 & 0x7FF) << 18) | ((word0 >> 11) & 0x3FFFF);
    }
    else {
        frame.id = (word0 & 0x7FF) | ((uint32_t)(frame.sid11 ? 1 : 0) << 11);
    }

    return frame;
}

int create_message_obj(uint8_t* dst, const CanFdFrame& frame, size_t* objectSize) {
    CanFdFrame txFrame = frame;
    MCP_TRY(validate_tx_frame(txFrame));
    MCP_TRY(finalize_frame_dlc(&txFrame));

    size_t rawSize = 8 + (size_t)txFrame.len;
    size_t objSize = (rawSize + 3) & ~static_cast<size_t>3; // round up to multiple of 4

    memset(dst, 0, objSize);
    store_word(dst, pack_id_word(txFrame));
    store_word(dst + 4, pack_control_word(txFrame));
    for (uint8_t i=0; i<txFrame.len; i++) {
        dst[8+i] = txFrame.data[i];
    }

    if (objectSize != NULL) {
        *objectSize = objSize;
    }
    return Error::None;
}

}  // namespace

MCP251863::MCP251863(spi_inst_t *ispi, uint iCSPin, uint iSTBYPin) {
    spi_                 = ispi;
    chipSelectPin_       = iCSPin;
    standbyPin_          = iSTBYPin;
    writeMode_           = WriteMode::WM_MCP_NORM;
    readMode_            = ReadMode::RM_MCP_NORM;
    txFifoNum_           = 1;
    rxFifoNum_           = 2;
    rxTimestampsEnabled_ = false;
    instance_             = this;
}

Error MCP251863::requireInitialized() const {
    return initialized_ ? Error::None : Error::NotInitialized;
}

// Checks if a FIFO was actually configured and in the direction expected
Error MCP251863::checkFifo(uint8_t fifoNum, bool wantTx) const {
    if (fifoNum < 1 || fifoNum > 32) {
        return Error::InvalidFifoNum;
    }
    const FifoInfo& info = fifoInfo[fifoNum];
    if (!info.configured || info.isTx != wantTx) {
        return Error::InvalidFifoNum;
    }
    return Error::None;
}

void MCP251863::csSelect() {
    asm volatile("nop \n nop \n nop");
    gpio_put(chipSelectPin_, 0);
    asm volatile("nop \n nop \n nop");
}

void MCP251863::csDeselect() {
    asm volatile("nop \n nop \n nop");
    gpio_put(chipSelectPin_, 1);
    asm volatile("nop \n nop \n nop");
}

int MCP251863::dmaWriteAddr(uint16_t startAddr, const uint8_t* data, size_t len) {
    if (fifo_op_state != FifoOperationState::IDLE) {
            return Error::Busy;
    }
    if (!pinsReady | !dmaInitialized) {
        return Error::NotInitialized;
    }
    if (data == nullptr && len > 0) {
        return Error::NullPointer;
    }
    // tx_buff is MAX_TRANSFER bytes; never write past it.
    if (len + 2 > MAX_TRANSFER) {
        return Error::PayloadTooLong;
    }
    if (writeMode_ != WriteMode::WM_MCP_NORM) {
        return Error::WrongMode;
    }
    
    // form message CCCC-AAAAAAAAAAAA
    tx_buff[0] = (to_underlying(Command::CMD_MCP_WRITA) << 4) | (startAddr >> 8);
    tx_buff[1] = (startAddr << 4) >> 4;

    memcpy(&tx_buff[2], data, len);

    transfer_len = len;

    spiTransferDMA(len + 2);

    return Error::None;
}

// blocking SPI
// CRC mode and Safe mode unimplemented
int MCP251863::writeAddr(uint16_t startAddr, const uint8_t* data, size_t len) {
    if (!pinsReady) {
        return Error::NotInitialized;
    }
    if (data == nullptr && len > 0) {
        return Error::NullPointer;
    }
    if (fifo_op_state != FifoOperationState::IDLE) {
            return Error::Busy;
    }
    // form message CCCC-AAAAAAAAAAAA
    uint8_t message[2];

    message[0] = (to_underlying(CMD_MCP_WRITA) << 4) | (startAddr >> 8);
    message[1] = (startAddr << 4) >> 4;

    csSelect()
    spi_write_blocking(spi_, message, 2);
    spi_write_blocking(spi_, data, len);
    csDeselect()

    return Error::None;
}

int MCP251863::dmaReadAddr(uint16_t startAddr, uint8_t* dst, size_t len) {
    if (fifo_op_state != FifoOperationState::IDLE) {
        return Error::Busy;
    }
    if (!pinsReady | !dmaInitialized) {
        return Error::NotInitialized;
    }
    if (dst == nullptr) {
        return Error::NullPointer;
    }
    if (len + 2 > MAX_TRANSFER) {
        return Error::PayloadTooLong;
    }
    if (readMode_ != ReadMode::RM_MCP_NORM) {
        return Error::WrongMode;
    }
    
    // form message CCCC-AAAAAAAAAAAA
    tx_buff[0] = (to_underlying(Command::CMD_MCP_READA) << 4) | (startAddr >> 8);
    tx_buff[1] = (startAddr << 4) >> 4;

    user_rx_buff = dst;

    transfer_len = len;

    spiTransferDMA(len + 2);

    return Error::None;
}

int MCP251863::readAddr(uint16_t startAddr, uint8_t* dst, size_t len) {
    if (!pinsReady_) {
        return Error::NotInitialized;
    }
    if (dst == nullptr) {
        return Error::NullPointer;
    }
    if (fifo_op_state != FifoOperationState::IDLE) {
        return Error::Busy;
    }
    // form message CCCC-AAAAAAAAAAAA
    message[0] = (to_underlying(Command::CMD_MCP_READA) << 4) | (startAddr >> 8);
    message[1] = (startAddr << 4) >> 4;

    csSelect()
    spi_write_blocking(spi_, message, 2);
    spi_read_blocking(spi_, 0, dst, len);
    csDeselect()

    return Error::None;
}

// helper functions for reading/writing to 4 byte registers
Error MCP251863::readReg(uint16_t addr, uint32_t* dst) {
    if (dst == nullptr) {
        return Error::NullPointer;
    }
    uint8_t buf[4] = {0};
    MCP_TRY(readAddr(addr, buf, 4));
    *dst = ((uint32_t)buf[0])       | 
           ((uint32_t)buf[1] << 8)  | 
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
    return Error::None;
}

Error MCP251863::writeReg(uint16_t addr, uint32_t value) {
    uint8_t buff[4] = {
        (uint8_t)(value & 0xFF),
        (uint8_t)((value >> 8) & 0xFF),
        (uint8_t)((value >> 16) & 0xFF),
        (uint8_t)((value >> 24) & 0xFF),
    };
    return writeAddr(addr, buff, 4);
}

// R-M-W 
Error MCP251863::updateByte(uint16_t addr, uint8_t field, uint8_t value) {
    uint8_t buff = 0;
    MCP_TRY(readAddr(addr, &buff, 1));
    buff = (uint8_t)((buff & (uint8_t)~field) | value);
    return writeAddr(addr, &buff, 1);
}

Error MCP251863::pollRegisterBit(
    uint16_t addr, uint8_t mask, bool wantSet, int maxIters, uint32_t delayUs, Error timeoutType) {
    for (int i = 0; i < maxIters; i++) {
        uint8_t buff = 0;
        MCP_TRY(readAddr(addr, &buff, 1));
        // if wantSet is false, then bit = 0 -> no error
        // if wantSet is true, then bit = 1 -> no error
        if (((buff & mask) != 0) == wantSet) {
            return Error::None;
        }
        sleep_us(delayUs);
    }
    stats.poll_timeouts++;
    return timeoutType;
}

Error MCP251863::pollRegisterBit(
    uint16_t addr, uint8_t mask, bool wantSet, Error timeoutType) {
        // default 100 iterations, 1 ms delay
        return pollRegisterBit(addr, mask, wantSet, 100, 1000, timeoutType);
    }


Error MCP251863::readOpMode(uint8_t* opmod) {
    uint8_t buff = 0;
    MCP_TRY(readAddr(to_underlying(RegisterAddress::REG_MCP_C1CON) + 2, &buff, 1));
    *opmod = buff >> 5;
    return Error::None;
}

Error MCP251863::waitForOpMode(ControllerMode target, int maxIters, uint32_t delayUs) {
    for (int i = 0; i < maxIters; i++) {
        uint8_t mode = 0;
        MCP_TRY(readOpMode(&mode));
        if (mode == to_underlying(target)) {
            return Error::None;
        }
        sleep_us(delayUs);
    }
    stats_.poll_timeouts++;
    return Error::ModeChangeTimeout;
}

Error MCP251863::waitForOpMode(ControllerMode target) {
    waitForOpMode(target, 100, 1000);
}

Error MCP251863::requireConfigMode() {
    uint8_t mode = 0;
    MCP_TRY(readOpMode(&mode));
    return (mode == to_underlying(ControllerMode::CMODE_MCP_CONF)) ? Error::None
                                                                    : Error::WrongMode;
}

Error MCP251863::readFifoUserAddress(
    uint16_t fifoPointAddr, size_t objectSize, uint16_t* messageAddr) {
    uint16_t ua = 0;
    MCP_TRY(readAddr(fifoPointAddr, (uint8_t*)&ua, 2));
    // RAM is 2 KB (0x400 - 0xBFF)
    // Thus buff value should be between 0x00 and 0x800 - objectSize
    if (ua > 0x800 - objectSize) {
        return Error::BadRegisterValue;
    }
    // add offset
    *messageAddr = ua + 0x400;
    return Error::None;
}

void MCP251863::initDMA() {
    if (dmaInitialized) {
        return Error::None;
    }
    int tx = dma_claim_unused_channel(false);
    if (tx < 0) {
        return Error::NoDmaChannel;
    }
    dma_tx_chan = tx;
    dma_tx_cfg = dma_channel_get_default_config(dma_tx_chan);
    channel_config_set_transfer_data_size(
        &dma_tx_cfg,
        DMA_SIZE_8
    );
    channel_config_set_read_increment(
        &dma_tx_cfg,
        true
    );
    channel_config_set_write_increment(
        &dma_tx_cfg,
        false
    );
    channel_config_set_dreq(
        &dma_tx_cfg,
        spi_get_dreq(spi_, true) 
    );  
    dma_channel_configure(
        dma_tx_chan,
        &dma_tx_cfg,
        &spi_get_hw(spi_)->dr,   
        tx_buff,   
        0, 
        false
    );

    int rx = dma_claim_unused_channel(false);
    if (rx < 0) {
        dma_channel_unclaim(tx);
        return Error::NoDmaChannel;
    }
    dma_rx_chan = rx;
    dma_rx_cfg = dma_channel_get_default_config(dma_rx_chan);
    channel_config_set_transfer_data_size(
        &dma_rx_cfg,
        DMA_SIZE_8
    );
    channel_config_set_read_increment(
        &dma_rx_cfg,
        false
    );
    channel_config_set_write_increment(
        &dma_rx_cfg,
        true
    );
    channel_config_set_dreq(
        &dma_rx_cfg,
        spi_get_dreq(spi_, false)
    );
    dma_channel_configure(
        dma_rx_chan,
        &dma_rx_cfg,
        rx_buff,
        &spi_get_hw(spi_)->dr, 
        0,
        false
    );

    // DMA raises IRQ line 0 when it finishes
    // call dmaIrqHandler whenever DMA int 0 fires
    dma_channel_set_irq0_enabled(
        dma_rx_chan, // RX finishes after TX
        true
    );
    // no one else should use the DMA_IRQ_0 line
    irq_set_exclusive_handler(
        DMA_IRQ_0,
        MCP251863::dmaIrqHandler
    );
    irq_set_enabled(
        DMA_IRQ_0,
        true
    );
    dmaInitialized = true;
    return Error::None;
}

void MCP251863::deinit() {
    initialized = false;
    if (!dmaInitialized) {
        return;
    }

    abortTransfer(); 
    dma_channel_set_irq0_enabled(dma_rx_chan, false);

    instance_ = nullptr;

    irq_remove_handler(DMA_IRQ_0, MCP251863::dmaIrqHandler);
    irq_set_enabled(DMA_IRQ_0, false);

    dma_channel_unclaim(dma_tx_chan);
    dma_channel_unclaim(dma_rx_chan);
    dma_tx_chan = -1;
    dma_rx_chan = -1;

    dmaInitialized = false;
}

void MCP251863::spiTransferDMA(size_t len) {
    // abort possible unfinished transactions
    dma_channel_abort(dma_tx_chan);
    dma_channel_abort(dma_rx_chan);

    // clear overrun and drain RX FIFO
    spi_get_hw(spi_)->icr = SPI_SSPICR_RORIC_BITS;
    while (spi_is_readable(spi_)) {
        (void)spi_get_hw(spi_)->dr;
    }

    dma_channel_set_read_addr(dma_tx_chan, tx_buff, false);
    dma_channel_set_write_addr(dma_rx_chan, rx_buff, false);

    dma_channel_set_trans_count(dma_tx_chan, len, false);
    dma_channel_set_trans_count(dma_rx_chan, len, false);

    transfer_start_us = time_us_32();

    gpio_put(chipSelectPin_, 0);

    // start both DMA channels
    dma_start_channel_mask(
        (1u << dma_tx_chan) | 
        (1u << dma_rx_chan)
    );
}

void MCP251863::dmaIrqHandler() {
    if (instance_ != nullptr) {
        dma_channel_acknowledge_irq0(instance_->dma_rx_chan); // clear int flag
        instance_->finishTransfer();
    }
}

void MCP251863::finishTransfer() {
    while (spi_is_busy(spi_)) {
        tight_loop_contents();
    }

    gpio_put(chipSelectPin_, 1);

    if (fifo_op_state == FifoOperationState::POPPING) {
        pending_fifo_uinc = true;
        memcpy(user_rx_buff, &rx_buff[2], transfer_len);
        user_frame.valid = 1;
        fifo_op_state = FifoOperationState::POP_DONE;
    }
    if (fifo_op_state == FifoOperationState::PUSHING) {
        pending_fifo_uinc = true;
        fifo_op_state = FifoOperationState::PUSH_DONE;
    }
}

void MCP251863::serviceFifoUinc() {
    MCP_TRY(updateByte(fifo_addr + 1, 0x00, 0b00000001));  // UINC
    pending_fifo_uinc = false;
    if (fifo_op_state == FifoOperationState::POP_DONE) {
        stats.frames_rx++;
    }
    return Error::None;
}

Error MCP251863::checkAsyncTimeout() {
    if (fifo_op_state == FifoOperationState::IDLE) {
        return Error::None;
    }
    if ((uint32_t)(time_us_32() - transfer_start_us_) > MCP251863_DMA_TIMEOUT_US) {
        abortTransfer();  
        stats_.dma_timeouts++;
        return Error::SpiTimeout;
    }
    return Error::None;
}

void MCP251863::abortTransfer() {
    if (dmaInitialized) {
        dma_channel_abort(dma_tx_chan);
        dma_channel_abort(dma_rx_chan);
    }
    if (pinsReady) {
        gpio_put(chipSelectPin_, 1);
    }

    fifo_op_state = FifoOperationState::IDLE;
    pending_fifo_uinc = false;
}

int MCP251863::init() {
    return init(default_init_config());
}

Error MCP251863::init(const InitConfig& config) {
    if ((config.txFifo < 1) || (config.txFifo > 32) || (config.rxFifo < 1) ||
        (config.rxFifo > 32) || (config.txFifo == config.rxFifo)) {
        return Error::InvalidFifoNum;
    }
    if ((config.txFifoDepth < 1) || (config.txFifoDepth > 32) ||
        (config.rxFifoDepth < 1) || (config.rxFifoDepth > 32)) {
        return Error::InvalidFifoDepth;
    }
    if (fifo_plsize_to_len(config.txPayloadSize) == 0 ||
        fifo_plsize_to_len(config.rxPayloadSize) == 0) {
        return Error::InvalidPayloadSize;
    }
    MCP_TRY(validate_bit_timing(config.nominalBitTiming, config.dataBitTiming));

    // Remember the config so recover() can repeat this exact bring-up
    lastConfig = config;
    haveConfig = true;
    initialized = false;

    if (!pinsReady) {
        gpio_init(chipSelectPin_);
        gpio_put(chipSelectPin_, 1);
        gpio_set_dir(chipSelectPin_, GPIO_OUT);
        gpio_init(standbyPin_);
        gpio_put(standbyPin_, to_underlying(TransceiverMode::TMODE_MCP_STBY));
        gpio_set_dir(standbyPin_, GPIO_OUT);
        pinsReady = true;
    }
    (void)setTransceiverMode(TransceiverMode::TMODE_MCP_STBY);

    writeMode_ = WriteMode::WM_MCP_NORM;
    readMode_  = ReadMode::RM_MCP_NORM;

    abortTransfer();
    for (FifoInfo& f : fifoInfo) {
        f = FifoInfo{};
    }
    bus_off_latched = false;
    // bits are cleared manually or when TXREQ is set
    txlarb_latched  = false;
    txerr_latched   = false;
    // could also expose: txbp, rxbp, txwarn, rxwarn, ewarn

    const Error err = configureDevice(config);
    if (err != Error::None) {
        initialized_ = false;
        (void)setTransceiverMode(TransceiverMode::TMODE_MCP_STBY);
        (void)reset();
        return err;
    }

    initialized_ = true;
    return Error::None;
}

Error MCP251863::configureDevice(const InitConfig& config) {
    uint8_t zero = 0;
    uint8_t one = 0;
    uint32_t reg = 0;

    MCP_TRY(reset());
    sleep_ms(10);

    MCP_TRY(pollRegisterBit(
        to_underlying(RegisterAddress::REG_MCP_OSC) + 1, 1 << 2, true,
        Error::OscillatorTimeout));

    MCP_TRY(setControllerMode(ControllerMode::CMODE_MCP_CONF));

    uint8_t osc = (config.enablePll ? 0x01 : 0x00) | (config.sclkDiv2 ? 0x10 : 0x00);
    MCP_TRY(writeAddr(to_underlying(RegisterAddress::REG_MCP_OSC), &osc, 1));
    if (config.enablePll) {
        // PLLRDY
        MCP_TRY(pollRegisterBit(
            to_underlying(RegisterAddress::REG_MCP_OSC) + 1, 0x01, true,
            Error::PllTimeout));
    }
    if (config.sclkDiv2) {
        // SCLKRDY
        MCP_TRY(pollRegisterBit(
            to_underlying(RegisterAddress::REG_MCP_OSC) + 1, 1 << 4, true,
            Error::SclkdivTimeout));
    }

    MCP_TRY(setBitTiming(config.nominalBitTiming, config.dataBitTiming));

    MCP_TRY(writeReg(
        to_underlying(RegisterAddress::REG_MCP_C1TDC),
        encode_tdc(config.enableTdc, config.tdcOffset)));

    txFifoNum_           = config.txFifo;
    rxFifoNum_           = config.rxFifo;
    rxTimestampsEnabled_ = config.rxTimestampEnable != 0;

    FifoInterruptFlag txFlags[] = {FIFO_INT_MCP_NFNE, FIFO_INT_MCO_TXAT};
    FifoInterruptFlag rxFlags[] = {FIFO_INT_MCP_NFNE, FIFO_INT_MCP_OVFL};
    MCP_TRY(initGeneralPurposeFifo(
        txFifoNum_,
        FifoMode::FIFO_MODE_MCP_TX,
        config.txPayloadSize,
        config.txFifoDepth,
        1, // prioNum
        TxRetransmitMode::TXRET_MCP_UNLIM,
        txFlags,
        2)); // intFlagSize
    MCP_TRY(initGeneralPurposeFifo(
        rxFifoNum_,
        FifoMode::FIFO_MODE_MCP_RX,
        config.rxPayloadSize,
        config.rxFifoDepth,
        0,
        TxRetransmitMode::TXRET_MCP_NONE,
        rxFlags,
        2));
    if (rxTimestampsEnabled_) {
        uint16_t rx_fifo_addr =
            to_underlying(RegisterAddress::REG_MCP_C1FIFOCONx) + 12 * (rxFifoNum_ - 1);
        MCP_TRY(updateByte(rx_fifo_addr, 0x00, 1 << 5));  // RXTSEN
    }

    // Disable filter 0 while programming it, then make it accept all frames
    // into rxFifoNum.
    uint16_t flt_ctrl_addr = to_underlying(RegisterAddress::REG_MCP_C1FLTCONx);
    MCP_TRY(writeAddr(flt_ctrl_addr, &zero, 1));
    MCP_TRY(writeReg(to_underlying(RegisterAddress::REG_MCP_C1FLTOBJx), 0));
    MCP_TRY(writeReg(to_underlying(RegisterAddress::REG_MCP_C1MASKx), 0)); // accepts all ids
    one = 0b10000000 | (rxFifoNum_ & 0b00011111); // enable filter 0 & send filter 0 msgs to FIFO 2 (RX)
    MCP_TRY(writeAddr(flt_ctrl_addr, &one, 1));

    // clear interrupt flag registers
    reg = 0xFFFFFFFF;
    MCP_TRY(writeReg(to_underlying(RegisterAddress::REG_MCP_C1RXIF), reg));
    MCP_TRY(writeReg(to_underlying(RegisterAddress::REG_MCP_C1TXIF), reg));
    MCP_TRY(writeReg(to_underlying(RegisterAddress::REG_MCP_C1RXOVIF), reg));
    MCP_TRY(writeReg(to_underlying(RegisterAddress::REG_MCP_C1TXATIF), reg));

    InterruptEnable interrupts[] = {
        INT_EN_MCP_RXIE, // RX
        INT_EN_MCP_TXIE, // TX
        INT_EN_MCP_RXOVIE, // RX overflow
        INT_EN_MCP_TXATIE, // transmit attempt
        INT_EN_MCP_CERRIE, // CAN bus error
        INT_EN_MCP_SERRIE, // system error
    };
    MCP_TRY(setInterrupts(interrupts, sizeof(interrupts) / sizeof(interrupts[0])));

    MCP_TRY(initDMA());

    MCP_TRY(setTransceiverMode(TransceiverMode::TMODE_MCP_NORM));
    MCP_TRY(setControllerMode(ControllerMode::CMODE_MCP_CFD_NORM));

    // wait for the mode switch to actually happen
    return waitForOpMode(ControllerMode::CMODE_MCP_CFD_NORM, 100, 1000);
}

Error MCP251863::recover() {
    if (!haveConfig) {
        return Error::NotInitialized;
    }
    const InitConfig cfg = lastConfig;
    const Error err = init(cfg);
    if (err == Error::None) {
        stats.recoveries++;
    }
    return err;
}

Error MCP251863::setBitTiming(BitTiming nominalTiming, BitTiming dataTiming) {
    if (!pinsReady) {
        return Error::NotInitialized;
    }
    MCP_TRY(validate_bit_timing(nominalTiming, dataTiming));
    
    MCP_TRY(requireConfigMode());

    MCP_TRY(writeReg(
        to_underlying(RegisterAddress::REG_MCP_C1NBTCFG), pack_nominal_bit_timing(nominalTiming)));

    return writeReg(
        to_underlying(RegisterAddress::REG_MCP_C1DBTCFG), pack_data_bit_timing(dataTiming));
}

Error MCP251863::reset() {
    if (!pinsReady) {
        return Error::NotInitialized;
    }
    if (fifo_op_state != FifoOperationState::IDLE) {
        return Error::Busy;
    }

    Command cmd = Command::CMD_MCP_RESET;
    uint8_t message[2] = {0};

    message[0] = to_underlying(cmd) << 4;

    csSelect();
    spi_write_blocking(spi_, message, 2);
    csDeselect();

    return Error::None;
}

Error MCP251863::initGeneralPurposeFifo(
    uint8_t fifoNum,
    FifoMode fifoMode,
    FifoPayloadSize plSize,
    uint8_t fSize,
    uint8_t prioNum,
    TxRetransmitMode retranMode,
    FifoInterruptFlag* intFlagArray,
    size_t intFlagSize) {
    if (fifoNum < 1 || fifoNum > 32) {
        return Error::InvalidFifoNum;
    }
    if (fSize < 1 || fSize > 32) {
        return Error::InvalidFifoDepth;
    }
    if (prioNum > 31) {
        return Error::InvalidArgument; 
    }
    uint8_t len = fifo_plsize_to_len(plSize);
    if (len == 0) {
        return Error::InvalidPayloadSize;
    }
    if (intFlagSize > 0 && intFlagArray == nullptr) {
        return Error::NullPointer;
    }

    uint8_t buff[4];
    uint16_t addr = to_underlying(RegisterAddress::REG_MCP_C1FIFOCONx) + 12 * (fifoNum - 1);

    uint8_t intFlags = 0;
    for (size_t i = 0; i < intFlagSize; i++) {
        intFlags |= static_cast<uint8_t>(intFlagArray[i]);
    }

    // wait for FRESET (bit 2 of the second byte) to clear
    MCP_TRY(pollRegisterBit(
        addr + 1, 0x04, false, Error::PollTimeout));

    buff[0] = intFlags | (to_underlying(fifoMode) << 7);
    buff[1] = 0b00000000;
    buff[2] = 0b00000000 | (to_underlying(retranMode) << 5) | prioNum;
    // FSIZE stores depth-1 (0 = 1 message; 31 = 32 messages); caller passes 1..32
    buff[3] = ((to_underlying(plSize) & 0b111) << 5) | ((fSize - 1) & 0x1F);

    MCP_TRY(writeAddr(addr, buff, 4));

    fifoInfo[fifoNum].configured   = true;
    fifoInfo[fifoNum].isTx         = (fifoMode == FifoMode::FIFO_MODE_MCP_TX);
    fifoInfo[fifoNum].fifoPayload  = len;
    return Error::None;
}

Error MCP251863::initTransmitEventFifo(
    uint8_t fSize, FifoInterruptFlag* intFlagArray, size_t intFlagSize) {
    if (fSize < 1 || fSize > 32) {
        return Error::InvalidFifoDepth;
    }
    if (intFlagSize > 0 && intFlagArray == nullptr) {
        return Error::NullPointer;
    }

    uint8_t buff[4];
    uint16_t addr = to_underlying(RegisterAddress::REG_MCP_C1TEFCON);

    uint8_t intFlags = 0;
    for (size_t i = 0; i < intFlagSize; i++) {
        intFlags |= static_cast<uint8_t>(intFlagArray[i]);
    }

    // wait for FRESET (bit 2 of the second byte) to clear
    MCP_TRY(pollRegisterBit(
        addr + 1, 0x04, false, Error::PollTimeout));

    buff[0] = 0b00000000 | intFlags;
    buff[1] = 0b00000000;
    buff[2] = 0b00000000;
    buff[3] = (fSize - 1) & 0x1F;

    return writeAddr(addr, buff, 4);
}

Error MCP251863::initTransmitQueue(
    FifoPayloadSize plSize,
    uint8_t fSize,
    uint8_t prioNum,
    TxRetransmitMode retranMode,
    FifoInterruptFlag* intFlagArray,
    size_t intFlagSize) {
    if (fSize < 1 || fSize > 32) {
        return Error::InvalidFifoDepth;
    }
    if (prioNum > 31) {
        return Error::InvalidArgument;
    }
    if (fifo_plsize_to_len(plSize) == 0) {
        return Error::InvalidPayloadSize;
    }
    if (intFlagSize > 0 && intFlagArray == nullptr) {
        return Error::NullPointer;
    }

    uint8_t buff[4];
    uint16_t addr = to_underlying(RegisterAddress::REG_MCP_C1TXQCON);

    uint8_t intFlags = 0;
    for (size_t i = 0; i < intFlagSize; i++) {
        intFlags |= static_cast<uint8_t>(intFlagArray[i]);
    }

    MCP_TRY(pollRegisterBit(
        addr + 1, 0x04, false, Error::PollTimeout));

    buff[0] = 0b00000000 | intFlags;
    buff[1] = 0b00000000;
    buff[2] = 0b00000000 | (to_underlying(retranMode) << 5) | prioNum;
    buff[3] = ((to_underlying(plSize) & 0b111) << 5) | ((fSize - 1) & 0x1F);

    return writeAddr(addr, buff, 4);
}

Error MCP251863::initFilter(uint8_t fltNum, uint8_t fifoNum, uint16_t canSID) {
    MCP_TRY(requireInitialized());
    if (fltNum > 31) {
        return Error::InvalidFilterNum;
    }
    MCP_TRY(checkFifo(fifoNum, false));  // must be a configured RX FIFO
    if (canSID > 0x7FF) {
        return Error::IdOutOfRange;
    }

    uint16_t flt_addr      = to_underlying(RegisterAddress::REG_MCP_C1FLTCONx) + fltNum;
    uint16_t flt_obj_addr  = to_underlying(RegisterAddress::REG_MCP_C1FLTOBJx) + 8 * fltNum;
    uint16_t flt_mask_addr = to_underlying(RegisterAddress::REG_MCP_C1MASKx) + 8 * fltNum;

    uint8_t buff[4];

    // CiFLTOBJm can only be modified while the filter is disabled 
    buff[0] = 0x00;
    MCP_TRY(writeAddr(flt_addr, buff, 1));

    // Supports standard ID only
    buff[0] = canSID & 0xFF;
    buff[1] = (canSID >> 8) & 0x0F;
    buff[2] = 0x00;
    buff[3] = 0x00;
    MCP_TRY(writeAddr(flt_obj_addr, buff, 4));

    // Set mask (so only the correct ID is accepted)
    buff[0] = 0xFF;        // MSID<7:0>
    buff[1] = 0x07;        // MSID<10:8>
    buff[2] = 0x00;        // MEID = don't care
    buff[3] = 0b01000000;  // MIDE = 1; MSID11 = 0 (don't care)
    MCP_TRY(writeAddr(flt_mask_addr, buff, 4));

    // Re-enable
    buff[0] = 0b00000000 | fifoNum | (1 << 7);
    return writeAddr(flt_addr, buff, 1);
}

Error MCP251863::start_send_canfd(
    uint32_t id, const uint8_t* data, size_t len, bool brs, bool extended_id) {
    return start_send_canfd(FifoIndex(txFifoNum_), id, data, len, brs, extended_id);
}

Error MCP251863::start_send_canfd(
    uint8_t fifoNum, uint32_t id, const uint8_t* data, size_t len, bool brs, bool extended_id) {
    MCP_TRY(requireInitialized());
        if (len > MCP251863_MAX_PAYLOAD) {
        return Error::PayloadTooLong;
    }
    auto dlc_opt = canfd_len_to_dlc(len);
    if (!dlc_opt) {
        return Error::InvalidPayloadLength;
    }
    if ((len > 0) && (data == nullptr)) {
        return Error::NullPointer;
    }

    CanFdFrame frame{};
    frame.id  = id;
    frame.ide = extended_id;
    frame.fdf = 1;
    frame.brs = brs;
    frame.len = len;
    frame.dlc = to_underlying(*dlc_opt);
    for (size_t i=0; i<len; i++) {
        frame.data[i] = data[i];
    }
    return start_send_frame(fifoNum, frame);
}

Error MCP251863::start_send_frame(const CanFdFrame& frame) {
    return start_send_frame(FifoIndex(txFifoNum_), frame);
}

Error MCP251863::start_send_frame(uint8_t fifoNum, const CanFdFrame& frame) {
    MCP_TRY(requireInitialized());

    MCP_TRY(checkFifo(fifoNum, true));

    // 8-byte header + up to 64 payload bytes, word padded => at most 72.
    uint8_t message[72];
    size_t objectSize = 0;
    MCP_TRY(create_message_obj(message, frame, &objectSize));

    if (frame.len > fifoInfo[fifoNum].payloadSize) {
        return Error::PayloadExceedsFifoSlot;
    }

    if (fifo_op_state != FifoOperationState::IDLE) {
        return Error::Busy;
    }

    uint16_t fifo_stat_addr =
        to_underlying(RegisterAddress::REG_MCP_C1FIFOSTAx) + 12 * (fifoNum - 1);
    uint16_t fifo_point_addr =
        to_underlying(RegisterAddress::REG_MCP_C1FIFOUAx) + 12 * (fifoNum - 1);
    fifo_addr = to_underlying(RegisterAddress::REG_MCP_C1FIFOCONx) + 12 * (fifoNum - 1);

    uint8_t stat = 0;
    MCP_TRY(readAddr(fifo_stat_addr, &stat, 1));
    if ((stat & 0b00000001) == 0) {
        stats.tx_fifo_full_events++;
        return Error::TxFifoFull;
    }

    uint16_t message_addr = 0;
    MCP_TRY(readFifoUserAddress(fifo_point_addr, objectSize, &message_addr));

    fifo_op_state = FifoOperationState::PUSHING;

    const Error err = dmaWriteAddr(message_addr, message, objectSize);
    if (err != Error::None) {
        fifo_op_state = FifoOperationState::IDLE;
    }
    return err;
}

Error MCP251863::poll_send() {
    MCP_TRY(requireInitialized());
    if (fifo_op_state == FifoOperationState::PUSH_DONE) {
        if (pending_fifo_uinc) {
            MCP_TRY(serviceFifoUinc());
        }
        return Error::None;
    }
    if (fifo_op_state == FifoOperationState::PUSHING) {
        MCP_TRY(checkAsyncTimeout());
        return Error::Busy;
    }
    return Error::NoOperationPending;
}

Error MCP251863::request_send() {
    return request_send(FifoIndex(txFifoNum_));
}

Error MCP251863::request_send(uint8_t fifoNum) {
    MCP_TRY(requireInitialized());

    MCP_TRY(checkFifo(fifoNum, true));

    uint16_t addr = to_underlying(RegisterAddress::REG_MCP_C1FIFOCONx) + 12 * (fifoNum - 1);

    MCP_TRY(pollRegisterBit(
        addr + 1, 0b00000001, false, Error::PollTimeout));
    MCP_TRY(updateByte(addr + 1, 0x00, 0b00000010));  // TXREQ

    stats_.frames_tx_requested++;
    return Error::None;
}

Error MCP251863::clear_send() {
    if (fifo_op_state == FifoOperationState::PUSHING) {
        return Error::Busy;
    }
    if (pending_fifo_uinc) {
        MCP_TRY(serviceFifoUinc());
    }
    fifo_op_state = FifoOperationState::IDLE;
    return Error::None;
}

Error MCP251863::start_read_canfd() { return start_read_frame(FifoIndex(rxFifoNum_)); }

Error MCP251863::start_read_canfd(uint8_t fifoNum) { return start_read_frame(fifoNum); }

Error MCP251863::start_read_frame() { return start_read_frame(FifoIndex(rxFifoNum_)); }

Error MCP251863::start_read_frame(uint8_t fifoNum) {
    MCP_TRY(requireInitialized());

    MCP_TRY(checkFifo(fifoNum, false));

    if (fifo_op_state != FifoOperationState::IDLE) {
        return Error::Busy;
    }

    fifo_addr = to_underlying(RegisterAddress::REG_MCP_C1FIFOCONx) + 12 * (fifoNum - 1);

    uint16_t fifo_stat_addr =
        to_underlying(RegisterAddress::REG_MCP_C1FIFOSTAx) + 12 * (fifoNum - 1);
    uint16_t fifo_point_addr =
        to_underlying(RegisterAddress::REG_MCP_C1FIFOUAx) + 12 * (fifoNum - 1);

    uint8_t buff = 0;
    uint16_t message_addr = 0;
    uint8_t header[12] = {0};
    size_t headerSize  = rxTimestampsEnabled_ ? 12 : 8;

    MCP_TRY(readAddr(fifo_stat_addr, &buff, 1));
    if ((buff & 0b00000001) == 0) {
        return Error::RxFifoEmpty;
    }

    MCP_TRY(readFifoUserAddress(fifo_point_addr, headerSize, &message_addr));
    MCP_TRY(readAddr(message_addr, header, headerSize));
    user_frame = decode_rx_header(header, rxTimestampsEnabled_);

    if (user_frame.len > 0) {
        fifo_op_state = FifoOperationState::POPPING;
        const Error err = dmaReadAddr(message_addr + headerSize, user_frame.data, user_frame.len);
        if (err != Error::None) {
            fifo_op_state = FifoOperationState::IDLE;
        }
        return err;
    }

    user_frame.valid = 1;
    pending_fifo_uinc = true;
    fifo_op_state = FifoOperationState::POP_DONE;
    return Error::None;
}

Error MCP251863::poll_read() {
    MCP_TRY(requireInitialized());

    if (fifo_op_state == FifoOperationState::POP_DONE) {
        if (pending_fifo_uinc) {
            MCP_TRY(serviceFifoUinc());
        }
        return Error::None;
    }
    if (fifo_op_state == FifoOperationState::POPPING) {
        MCP_TRY(checkAsyncTimeout());
        return Error::Busy;
    }
    return Error::NoOperationPending;
}

Result<CanFdFrame> MCP251863::get_read_frame() {
    MCP_TRY(poll_read());
    fifo_op_state = FifoOperationState::IDLE;
    return user_frame;
}

Result<FifoStatus> MCP251863::getFIFOStatus(uint8_t fifoNum) {
    MCP_TRY(requireInitialized());

    FifoStatus status{};

    uint16_t fifo_stat_addr =
        to_underlying(RegisterAddress::REG_MCP_C1FIFOSTAx) + 12 * (fifoNum - 1);
    uint32_t reg = 0;
    MCP_TRY(readReg(fifo_stat_addr, &reg));

    status.fifo_index = (reg >> 8) & 0x1F;
    status.tx_aborted = (reg & (1UL << 7)) != 0;
    status.tx_lost_arbitration = (reg & (1UL << 6)) != 0;
    status.tx_error = (reg & (1UL << 5)) != 0;
    status.tx_attempts_exhausted = (reg & (1UL << 4)) != 0;
    status.rx_overflow = (reg & (1UL << 3)) != 0;
    status.empty_or_full = (reg & (1UL << 2)) != 0;
    status.half_empty_or_half_full = (reg & (1UL << 1)) != 0;
    status.not_full_or_not_empty = (reg & 1UL) != 0;

    return status;
}

Result<Status> MCP251863::getStatus() {
    MCP_TRY(requireInitialized());

    Status status{};

    MCP_TRY(readReg(to_underlying(RegisterAddress::REG_MCP_C1INT), &status.interrupt_flags));
    MCP_TRY(readReg(to_underlying(RegisterAddress::REG_MCP_C1RXIF), &status.rx_if));
    MCP_TRY(readReg(to_underlying(RegisterAddress::REG_MCP_C1TXIF), &status.tx_if));
    MCP_TRY(readReg(to_underlying(RegisterAddress::REG_MCP_C1RXOVIF), &status.rx_overflow_if));
    MCP_TRY(readReg(to_underlying(RegisterAddress::REG_MCP_C1TXATIF), &status.tx_attempt_if));
    MCP_TRY(readReg(to_underlying(RegisterAddress::REG_MCP_C1TREC), &status.trec));
    MCP_TRY(readReg(to_underlying(RegisterAddress::REG_MCP_C1BDIAGx), &status.bdiag0));
    MCP_TRY(readReg(to_underlying(RegisterAddress::REG_MCP_C1BDIAGx) + 4, &status.bdiag1));
    MCP_TRY(readReg(to_underlying(RegisterAddress::REG_MCP_CRC), &status.crc));

    status.bus_off = (status.trec & (1UL << 21)) != 0;
    status.tx_error_passive = (status.trec & (1UL << 20)) != 0;
    status.rx_error_passive = (status.trec & (1UL << 19)) != 0;
    status.tx_error_warning = (status.trec & (1UL << 18)) != 0;
    status.rx_error_warning = (status.trec & (1UL << 17)) != 0;
    status.error_warning = (status.trec & (1UL << 16)) != 0;
    status.tx_error_count = (status.trec >> 8) & 0xFF;
    status.rx_error_count = status.trec & 0xFF;
    status.spi_crc_format_error = (status.crc & (1UL << 17)) != 0;
    status.spi_crc_error = (status.crc & (1UL << 16)) != 0;

    return status;
}

Error MCP251863::pollFaults() {
    MCP_TRY(requireInitialized());

    Error worst = Error::None;
    uint8_t b = 0;

    const uint16_t tx_sta =
        to_underlying(RegisterAddress::REG_MCP_C1FIFOSTAx) + 12 * (txFifoNum_ - 1);
    MCP_TRY(readAddr(tx_sta, &b, 1));
    const bool larb = (b & (1 << 6)) != 0;  // TXLARB
    const bool terr = (b & (1 << 5)) != 0;  // TXERR
    const bool tatt = (b & (1 << 4)) != 0;  // TXATIF (R/C)
    if (larb && !txlarb_latched_) {
        stats.tx_arbitration_losses++;
    }
    if (terr && !txerr_latched_) {
        stats.tx_errors++;
    }
    txlarb_latched_ = larb;
    txerr_latched_  = terr;
    if (tatt) {
        stats_.tx_attempts_exhausted++;
        b = (uint8_t)(b & ~(1 << 4));
        MCP_TRY(writeAddr(tx_sta, &b, 1));
        worst = Error::TxAttemptsExhausted;
    }

    const uint16_t rx_sta =
        to_underlying(RegisterAddress::REG_MCP_C1FIFOSTAx) + 12 * (rxFifoNum_ - 1);
    MCP_TRY(readAddr(rx_sta, &b, 1));
    if (b & (1 << 3)) {  // RXOVIF (R/C)
        stats_.rx_overflows++;
        b = (uint8_t)(b & ~(1 << 3));
        MCP_TRY(writeAddr(rx_sta, &b, 1));
        worst = Error::RxOverflow;
    }

    // const uint16_t crc_b2 = to_underlying(RegisterAddress::REG_MCP_CRC) + 2;
    // MCP_TRY(readAddr(crc_b2, &b, 1));
    // if (b & 0x03) {
    //     if (b & 0x01) {
    //         stats_.spi_crc_errors_device++;
    //     }
    //     if (b & 0x02) {
    //         stats_.spi_crc_format_errors++;
    //     }
    //     b = (uint8_t)(b & ~0x03);
    //     MCP_TRY(writeAddr(crc_b2, &b, 1));
    //     worst = Error::CrcMismatch;
    // }

    // --- ECC (ECCSTAT byte 0: bit 1 SECIF, bit 2 DEDIF) --------------------
    // Only ever set if ECC is enabled in ECCCON, which this driver does not
    // currently do, so these counters stay 0 until it does.
    // const uint16_t ecc = to_underlying(RegisterAddress::REG_MCP_ECCSTAT);
    // MCP_TRY(readAddr(ecc, &b, 1));
    // if (b & 0x06) {
    //     const bool ded = (b & 0x04) != 0;
    //     if (b & 0x02) {
    //         stats_.ecc_single_corrections++;
    //     }
    //     if (ded) {
    //         stats_.ecc_double_errors++;
    //     }
    //     b = (uint8_t)(b & ~0x06);
    //     MCP_TRY(writeAddr(ecc, &b, 1));
    //     if (ded) {
    //         worst = Error::EccError;
    //     }
    // }

    uint32_t trec = 0;
    MCP_TRY(readReg32(to_underlying(RegisterAddress::REG_MCP_C1TREC), &trec));
    const bool busOff = (trec & (1UL << 21)) != 0;
    if (busOff && !bus_off_latched_) {
        stats_.bus_off_events++;
    }
    bus_off_latched_ = busOff;
    if (busOff) {
        worst = Error::BusOff;
    }

    return worst;
}

Error MCP251863::setControllerMode(ControllerMode contMode) {
    const uint16_t addr = to_underlying(RegisterAddress::REG_MCP_C1CON);

    uint8_t current = 0;
    MCP_TRY(readOpMode(&current));

    if (current != to_underlying(ControllerMode::CMODE_MCP_CONF)) {
        MCP_TRY(updateByte(addr + 3, 0b00000111, to_underlying(ControllerMode::CMODE_MCP_CONF)));
        MCP_TRY(waitForOpMode(ControllerMode::CMODE_MCP_CONF, MCP251863_POLL_MAX_ITERS, 100));
    }

    return updateByte(addr + 3, 0b00000111, to_underlying(contMode));
}

Error MCP251863::setTransceiverMode(TransceiverMode mode) {
    if (!pinsReady_) {
        return Error::NotInitialized;
    }
    gpio_put(standbyPin_, to_underlying(mode));
    return Error::None;
}

Error MCP251863::setInterrupts(InterruptEnable* intEnArray, size_t intEnSize) {
    if (intEnSize > 32) {
        return Error::InvalidArgument;
    }
    if (intEnSize > 0 && intEnArray == nullptr) {
        return Error::NullPointer;
    }
    uint32_t message = 0;
    for (size_t i = 0; i < intEnSize; i++) {
        message |= static_cast<uint32_t>(intEnArray[i]);
    }
    return writeReg(to_underlying(RegisterAddress::REG_MCP_C1INT), message);
}

Error MCP251863::setPinMode(IoPin pin, IoMode mode) {
    MCP_TRY(requireInitialized());

    uint8_t buff[4] = {0};
    MCP_TRY(readAddr(to_underlying(RegisterAddress::REG_MCP_IOCON), buff, 4));
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
            default:
                return Error::InvalidArgument;
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
            default:
                return Error::InvalidArgument;
        }
    } else {
        return Error::InvalidArgument;
    }
    return writeAddr(to_underlying(RegisterAddress::REG_MCP_IOCON), buff, 4);
}

// C1VEC layout
// 31:24 rxcode (third byte)
// 23:16 txcode (second byte)
// 12:8 filhit (first byte, + some offset stuff)
// 7:0 icode (zeroth byte)

Result<int> MCP251863::getTXCode() {
    MCP_TRY(requireInitialized());
    uint8_t buff = 0;
    MCP_TRY(readAddr(to_underlying(RegisterAddress::REG_MCP_C1VEC) + 2, &buff, 1));
    return static_cast<int>(buff & 
        0x7F);
}

Result<int> MCP251863::getRXCode() {
    MCP_TRY(requireInitialized());
    uint8_t buff = 0;
    MCP_TRY(readAddr(to_underlying(RegisterAddress::REG_MCP_C1VEC) + 3, &buff, 1));
    return static_cast<int>(buff & 0x7F);
}

Result<int> MCP251863::getFLTCode() {
    MCP_TRY(requireInitialized());
    // its 5 bits so we mask with 0x1F instead of 0x7F
    uint8_t buff = 0;
    MCP_TRY(readAddr(to_underlying(RegisterAddress::REG_MCP_C1VEC) + 1, &buff, 1));
    return static_cast<int>(buff & 0x1F);
}

Result<int> MCP251863::getICode() {
    MCP_TRY(requireInitialized());
    uint8_t buff = 0;
    MCP_TRY(readAddr(to_underlying(RegisterAddress::REG_MCP_C1VEC), &buff, 1));
    buff &= 0x7F;
    if (buff > 0b1001010) {  // max valid ICODE value; this used to be `return -1`
        return Error::BadRegisterValue;
    }
    return static_cast<int>(buff);
}
