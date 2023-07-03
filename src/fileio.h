#pragma once

#include <string>
#include <cstdint>

class FileIO
{
public:
	FileIO();
	~FileIO();

	void Close();

	const bool OpenWrite(const std::string& filename);
	const bool OpenRead(const std::string& filename);

	void WriteInt8(const int8_t val);
	void WriteUInt8(const uint8_t val);
	void WriteInt32(const int32_t val);
	void WriteInt64(const int64_t val);
	void WriteStr(const std::string& val);

	const bool ReadInt8(int8_t& val);
	const bool ReadUInt8(uint8_t& val);
	const bool ReadInt32(int32_t& val);
	const bool ReadInt64(int64_t& val);
	const bool ReadStr(std::string& val, const int len);

private:

	FILE* m_file;

};