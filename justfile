set default-list

mod elec

mod telem

mod cev-lib

# build and test everything ci does (telem/shm needs a pi, excluded)
check:
    just elec build
    just cev-lib rp2040_mcp251863 build
    just cev-lib chuds test
