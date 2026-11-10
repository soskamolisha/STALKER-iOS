#pragma once
// Minimal compile-time adapter for the standalone upstream crc32.cpp probe.
// This is NOT the full OpenXRay precompiled header or a replacement for xrCore.
#include <cstdint>
using u8 = std::uint8_t;
using u32 = std::uint32_t;
#ifndef _DELIMITER
#define _DELIMITER '\\'
#endif
