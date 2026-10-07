#include <cstdio>

#include "chuds/frame.hpp"

// ip link takes a sample point as a fraction, so print it as 0.NN
static_assert(chuds::kNominalSamplePoint < 100 && chuds::kDataSamplePoint < 100);

// prints the ip link arguments that run a SocketCAN interface at the chuds bus settings
int main() {
    std::printf(
        "bitrate %u sample-point 0.%02u dbitrate %u dsample-point 0.%02u fd on restart-ms 100\n",
        static_cast<unsigned>(chuds::kNominalBitrate),
        static_cast<unsigned>(chuds::kNominalSamplePoint),
        static_cast<unsigned>(chuds::kDataBitrate), static_cast<unsigned>(chuds::kDataSamplePoint));
}
