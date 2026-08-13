#pragma once

#include <optional>

struct ApplicationInfo {
  std::optional<const char*> applicationName;
  std::optional<uint32_t> applicationVersion;
  std::optional<const char*> engineName;
  std::optional<uint32_t> engineVersion;
  std::optional<uint32_t> apiVersion;
};
