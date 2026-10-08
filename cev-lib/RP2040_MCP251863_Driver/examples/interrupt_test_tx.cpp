#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "mcp251863.hpp"

static constexpr uint SPI_SCK  = 22;
static constexpr uint SPI_TX   = 23;
static constexpr uint SPI_RX   = 20;
static constexpr uint CS_PIN   = 21;
static constexpr uint STBY_PIN = 24;

int main() {
    stdio_init_all();
    sleep_ms(10000);
    printf("Interrupts TX example starting\n");

    spi_init(spi0, MCP251863_BAUD_RATE);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(SPI_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_TX,  GPIO_FUNC_SPI);
    gpio_set_function(SPI_RX,  GPIO_FUNC_SPI);

    MCP251863 mcp(spi0, CS_PIN, STBY_PIN);

    if (mcp.init() != Error::None) {
        printf("init failed\n");
        return 1;
    }
    printf("init ok\n");

    mcp.configureTxFifo(0);

    mcp.begin();

    uint8_t data[8] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    uint32_t id = 0x123;

    int count = 0;

    for (;;) {
        data[0] = (uint8_t)(count & 0xFF);

        // Queue FIFO 0 msg & request send
        while (mcp.start_push_canfd(0, id, data, sizeof(data), true) != Error::None) {
            tight_loop_contents();
        }
        while (mcp.poll_push() == Error::Busy) {
            tight_loop_contents();
        }
        mcp.clear_send();
        if (mcp.request_send(0) == Error::None) {
            printf("sent FIFO0 frame %d id = 0x%03X data[0]=%02X\n", count, id, data[0]);
        }

        // Polling information
        if (count % 10 == 0) {
            Status s = mcp.getStatus().value();
            printf("bus_off=%d tx_err=%d tx_count=%d\n",
                s.bus_off, s.tx_error_passive, s.tx_error_count);
        }

        count++;
        sleep_ms(5000); // wait 5 whole seconds before sending another message
    }

    return 0;
}