#ifndef MESSAGE_ID_HPP
#define MESSAGE_ID_HPP

#include <cstdint>
#include <string>

std::string createMessageId(std::uint64_t boot_id, std::uint64_t sequence);

#endif // MESSAGE_ID_HPP
