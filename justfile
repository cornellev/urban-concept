set default-list

# use powershell on windows
set windows-shell := ["powershell.exe", "-NoProfile", "-Command"]

mod elec

mod telem

mod cev-lib

# linux-only chuds targets (socketcan tests, chuds_demo) are skipped off linux
# build, test, and heap-audit like ci (telem/shm needs a pi, excluded)
# ci also runs fmt-check and lint as separate jobs
check:
    just elec build
    just elec audit-heap
    just cev-lib rp2040_mcp251863 build
    just cev-lib chuds test

# our sources, excluding the vendored driver
fmt_files := replace(`git ls-files -- 'cev-lib/chuds/*.cpp' 'cev-lib/chuds/*.hpp' 'elec/src/*.cpp' 'elec/src/*.hpp' 'elec/src/*.h'`, "\n", " ")

# fail if our code is not clang-format clean
fmt-check:
    clang-format --dry-run --Werror {{ fmt_files }}

# clang-format our code in place
fmt:
    clang-format -i {{ fmt_files }}
