set default-list

# use powershell on windows
set windows-shell := ["powershell.exe", "-NoProfile", "-Command"]

mod elec

mod telem

mod cev-lib

# linux-only chuds targets (socketcan tests, chuds_demo) are skipped off linux
# build and test what ci does (telem/shm needs a pi, excluded)
check:
    just elec build
    just cev-lib rp2040_mcp251863 build
    just cev-lib chuds test
