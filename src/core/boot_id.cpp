#include "core/boot_id.hpp"

#include <fstream>
#include <iostream>

#include "core/config.hpp"

bool loadAndIncrementBootId(std::uint64_t& boot_id)
{
  boot_id = 0;

  std::ifstream input(Config::BOOT_ID_PATH);

  if (input.is_open())
  {
    input >> boot_id;

    if (input.fail())
    {
      std::cerr << "Failed to read boot_id\n";
      return false;
    }
  }

  ++boot_id;

  std::ofstream output(Config::BOOT_ID_PATH, std::ios::trunc);

  if (!output.is_open())
  {
    std::cerr << "Failed to write boot_id\n";
    return false;
  }

  output << boot_id << '\n';

  return true;
}
