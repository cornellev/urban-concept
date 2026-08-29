# urban-concept

proposed file layout:

```
urban-concept/
├── elec/
│   ├── pico-sdk/ vendored from raspberrypi
│   ├── CMake config
│   ├── build/              (gitignored)
│   │   └── flash binaries  → .uf2
│   └── src/
│       ├── Front Aux/
│       ├── Back Aux/
│       └── other projects
├── telem/               sensor data collection, publishing data via ros2 on the pi, tailscale, PPP
├── auto/                if they have anything they want to add
└── cev-lib/
    └── CHUDS/ includes the rp2040 driver
```
