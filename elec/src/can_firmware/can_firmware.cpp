#include <cstdint>
#include <cstdio>
#include <cstring>

#include "hardware/spi.h"
#include "mcp251863.h"
#include "pico/stdlib.h"

constexpr uint8_t rx_fifo_num = 1;
constexpr uint8_t tx_fifo_num = 2;
constexpr uint8_t filter_num = 0;

constexpr uint32_t PI_ID = 1;
constexpr uint32_t BOARD_ID = 2;

// 12 bytes bc i think ethan said so
constexpr uint32_t request_payload_size = 12;
constexpr uint32_t response_payload_size = 12;

// spi pin configs
// TODO: adjust to match actual pcb
constexpr uint SPI_SCK_PIN = 18;
constexpr uint SPI_MOSI_PIN = 19;
constexpr uint SPI_MISO_PIN = 16;
constexpr uint MCP_CS_PIN = 17;
constexpr uint MCP_STBY_PIN = 15;

int main() {
    stdio_init_all();

    // spi hardware init
    spi_init(spi0, MCP251863_BAUD_RATE);
    gpio_set_function(SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MISO_PIN, GPIO_FUNC_SPI);

    MCP251863 mcp{spi0, MCP_CS_PIN, MCP_STBY_PIN};

    InitConfig config = {};
    config.enablePll = 0;
    config.sclkDiv2 = 0;
    config.enableTdc = 1;
    config.rxTimestampEnable = 0;
    config.tdcOffset = 6;
    config.txFifo = tx_fifo_num;
    config.rxFifo = rx_fifo_num;
    config.txFifoDepth = 8;
    config.rxFifoDepth = 8;
    config.txPayloadSize = PayloadSize::PL_SIZE_MCP_12;
    config.rxPayloadSize = PayloadSize::PL_SIZE_MCP_12;
    config.nominalBitTiming = kBitTiming500K40MHz;
    config.dataBitTiming = kBitTiming2M40MHz;

    if (!mcp.init(config)) {
        printf("MCP251863 init failed\n");
        while (true) {
            sleep_ms(1000);
        }
    }

    // only accept messages addressed to PI_ID, route them into rx_fifo_num
    mcp.initFilter(filter_num, rx_fifo_num, PI_ID);

    for (;;) {
        // zeroed payload just to indicate that we're requesting sensor data
        uint8_t request_payload[request_payload_size] = {};
        mcp.send_canfd(tx_fifo_num, BOARD_ID, request_payload, request_payload_size, true);

        // Wait for response
        bool ok = false;

        while (!ok) {
            CanFdFrame response = mcp.read_frame(rx_fifo_num);
            ok = response.valid && response.len >= response_payload_size;

            if (ok) {
                uint32_t ts;
                float value1, value2;
                memcpy(&ts, response.data + 0, sizeof(uint32_t));
                memcpy(&value1, response.data + 4, sizeof(float));
                memcpy(&value2, response.data + 8, sizeof(float));

                printf("ts=%lu v1=%f v2=%f\n", (unsigned long)ts, (double)value1, (double)value2);
            }
        }
    }

    return 0;
}
