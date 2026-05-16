// Binary file serialization utilties
#include <cstring>
#include <cfloat>
#include <string>

#include "asset/Binary.hpp"

uint8_t ReadU8(const std::string_view& str, size_t* offset, bool* fileTooSmallPtr)
{
	if (*fileTooSmallPtr || str.size() - 1 < *offset)
	{
		*fileTooSmallPtr = true;
		return UINT8_MAX;
	}

	uint8_t u8 = 0;
	memcpy(&u8, str.data() + *offset, sizeof(uint8_t));
	(*offset)++;

	return u8;
}

uint16_t ReadU16(const std::string_view& vec, size_t offset, bool* fileTooSmallPtr)
{
	if (*fileTooSmallPtr || vec.size() < offset + 2)
	{
		*fileTooSmallPtr = true;
		return UINT16_MAX;
	}

	uint16_t u16 = 0;
	memcpy(&u16, vec.data() + offset, sizeof(uint16_t));

	return u16;
}

uint16_t ReadU16(const std::string_view& vec, size_t* offset, bool* fileTooSmallPtr)
{
	uint16_t u16 = ReadU16(vec, *offset, fileTooSmallPtr);
	*offset += 2ull;

	return u16;
}

uint32_t ReadU32(const std::string_view& vec, size_t offset, bool* fileTooSmallPtr)
{
	if (*fileTooSmallPtr || vec.size() < offset + 4)
	{
		*fileTooSmallPtr = true;
		return UINT32_MAX;
	}

	uint32_t u32 = 0;
	memcpy(&u32, vec.data() + offset, sizeof(uint32_t));

	return u32;
}

uint32_t ReadU32(const std::string_view& vec, size_t* offset, bool* fileTooSmallPtr)
{
	uint32_t u32 = ReadU32(vec, *offset, fileTooSmallPtr);
	*offset += 4ull;

	return u32;
}

float ReadF32(const std::string_view& vec, size_t* offset, bool* fileTooSmallPtr)
{
	if (*fileTooSmallPtr || vec.size() < (*offset) + 4)
	{
		*fileTooSmallPtr = true;
		return FLT_MAX;
	}

	float f32 = 0.f;
	memcpy(&f32, vec.data() + *offset, sizeof(float));

	*offset += 4ull;

	return f32;
}

void WriteU8(std::string& vec, uint8_t v)
{
    char c = 0;
    memcpy(&c, &v, sizeof(v));
    vec.push_back(c);
}

void WriteU16(std::string& vec, uint16_t v)
{
	vec.resize(vec.size() + 2);
	memcpy(&vec[vec.size() - 2], &v, sizeof(uint16_t));
}

void WriteU32(std::string& vec, uint32_t v)
{
	vec.resize(vec.size() + 4);
	memcpy(&vec[vec.size() - 4], &v, sizeof(uint32_t));
}

void WriteF32(std::string& vec, float v)
{
	WriteU32(vec, std::bit_cast<uint32_t, float>(v));
}
