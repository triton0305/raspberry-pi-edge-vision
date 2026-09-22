#ifndef CONFIG_HPP
#define CONFIG_HPP

namespace Config
{
constexpr const char* DEVICE_ID = "vision-pi-01";
constexpr const char* BOOT_ID_PATH = "/home/triton/projects/edge_vision/boot_id.dat";
constexpr int PROTOCOL_VERSION = 1;
constexpr int ACK_TIMEOUT_MS = 1500;
constexpr int MAX_RETRY_COUNT = 2;
}

#endif // CONFIG_HPP