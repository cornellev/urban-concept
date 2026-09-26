set default-list

# use powershell on windows
set windows-shell := ["powershell.exe", "-NoProfile", "-Command"]

mod elec

mod telem

mod cev-lib

# linux-only chuds targets (socketcan tests, chuds_demo) are skipped off linux
# ci also checks formatting with treefmt (nix fmt) and runs lint as separate jobs
# build and test like ci (telem/shm needs a pi, excluded)
check:
    just elec build
    just cev-lib chuds test
