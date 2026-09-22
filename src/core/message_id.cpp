#include "core/message_id.hpp"

#include <iomanip>
#include <sstream>

#include "core/config.hpp"

std::string createMessageId(std::uint64_t boot_id, std::uint64_t sequence)
{
  std::ostringstream stream;

  stream << Config::DEVICE_ID << '-'
         << std::setfill('0') << std::setw(6) << boot_id << '-'
         << std::setfill('0') << std::setw(8) << sequence;

  return stream.str();
}
