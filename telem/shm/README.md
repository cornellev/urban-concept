# shm

SPI sensor reader to shared memory, with a Python reader.

`write_shm.cpp` is a Pi-side daemon: it polls the RP2040 sensor boards over SPI
(HDLC-framed, CRC-checked), reads GPS over UART, and publishes the latest
snapshot into POSIX shared memory `/sensor_shm` using a seqlock so readers never
block. `read_shm.py` reads that block lock-free.

The two are coupled only by the shm name (`/sensor_shm`) and the snapshot struct
layout - keep them in sync.

## Build and run (Pi only)

Needs `pigpiod` running and the pigpio dev headers. Does not build on macOS
(depends on `pigpiod_if2`, `linux/spi/spidev.h`).

```
cmake -S telem/shm -B telem/shm/build -G Ninja
cmake --build telem/shm/build
sudo pigpiod              # if not already running
./telem/shm/build/write_shm
```

Then in another shell:

```
python3 telem/shm/read_shm.py
```
