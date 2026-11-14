#pragma once
#include <string>
#include <vector>

namespace stalker {
// Lists unpacked level directories only; packed archives require a separate reader.
std::vector<std::string> discoverLooseLevels(const std::string& gameRoot);
}
