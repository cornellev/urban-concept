set default-list

# configure the elec cmake
[group("elec")]
configure-elec:
    cmake -S elec -B elec/build -G Ninja

# build every elec board
[group("elec")]
build-elec-all: configure-elec
    cmake --build elec/build

# build one elec board
[group("elec")]
build-elec target: configure-elec
    cmake --build elec/build --target {{ target }}

# clean elec build
[group("elec")]
clean-elec:
    rm -rf elec/build

# telem currently has nothing

# auto currently has nothing
