// Binary file serialization utilties
#pragma once

#include <string_view>
#include <cstdint>

uint8_t ReadU8(const std::string_view& str, size_t* offset, bool* fileTooSmallPtr);
uint16_t ReadU16(const std::string_view& vec, size_t offset, bool* fileTooSmallPtr);
uint16_t ReadU16(const std::string_view& vec, size_t* offset, bool* fileTooSmallPtr);
uint32_t ReadU32(const std::string_view& vec, size_t offset, bool* fileTooSmallPtr);
uint32_t ReadU32(const std::string_view& vec, size_t* offset, bool* fileTooSmallPtr);
float ReadF32(const std::string_view& vec, size_t* offset, bool* fileTooSmallPtr);

void WriteU8(std::string& str, uint8_t v);
void WriteU16(std::string& vec, uint16_t v);
void WriteU32(std::string& vec, uint32_t v);
void WriteF32(std::string& vec, float v);
