#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstddef>

namespace Config
{
constexpr const char* DEVICE_ID = "vision-pi-01";
constexpr const char* BOOT_ID_PATH = BOOT_ID_FILE_PATH;

constexpr int PROTOCOL_VERSION = 1;
constexpr int ACK_TIMEOUT_MS = 1500;
constexpr int MAX_RETRY_COUNT = 2;
constexpr std::size_t MAX_QUEUE_SIZE = 16;
}

#endif // CONFIG_HPP