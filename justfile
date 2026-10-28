set default-list

# use powershell on windows
set windows-shell := ["powershell.exe", "-NoProfile", "-Command"]

mod elec

mod telem

mod cev-lib

mod auto

# linux-only chuds targets (socketcan tests, chuds_demo) are skipped off linux
# ci also checks formatting with treefmt (nix fmt) and runs lint as separate jobs
# build and test like ci (telem/shm needs a pi, excluded)
check:
    just elec build
    just test

# test every module
test:
    just cev-lib chuds test
    just auto chuds_zenoh test

# lint every module
lint:
    just elec lint
    just cev-lib chuds lint
    just auto chuds_zenoh lint
