#include "fileio.h"

#include <vector>

FileIO::FileIO() :m_file(nullptr)
{

}

FileIO::~FileIO()
{
	Close();
}

void FileIO::Close()
{
	if (m_file)
	{
		fclose(m_file);
		m_file = nullptr;
	}
}

const bool FileIO::OpenRead(const std::string& filename)
{
	errno_t err = fopen_s(&m_file, filename.c_str(), "rb");
	return err == 0;
}

const bool FileIO::OpenWrite(const std::string& filename)
{
	errno_t err = fopen_s(&m_file, filename.c_str(), "wb");
	return err == 0;
}

const bool FileIO::ReadInt8(int8_t& val)
{
	if (m_file)
	{
		size_t len = fread(&val, 1, 1, m_file);
		return len == 1;
	}
	return false;
}

const bool FileIO::ReadUInt8(uint8_t& val)
{
	if (m_file)
	{
		size_t len = fread(&val, 1, 1, m_file);
		return len == 1;
	}
	return false;
}

const bool FileIO::ReadInt32(int32_t& val)
{
	if (m_file)
	{
		std::vector<uint8_t> buff(4, 0);
		size_t len = fread(&buff[0], 1, buff.size(), m_file);
		if (len == 4)
		{
			val = buff[0] << 24;
			val |= buff[1] << 16;
			val |= buff[2] << 8;
			val |= buff[3];
			return true;
		}
	}
	return false;
}

const bool FileIO::ReadInt64(int64_t& val)
{
	if (m_file)
	{
		std::vector<uint8_t> buff(8, 0);
		size_t len = fread(&buff[0], 1, buff.size(), m_file);
		if (len == 8)
		{
			val = static_cast<int64_t>(buff[0]) << 56;
			val |= static_cast<int64_t>(buff[1]) << 48;
			val |= static_cast<int64_t>(buff[2]) << 40;
			val |= static_cast<int64_t>(buff[3]) << 32;
			val |= static_cast<int64_t>(buff[4]) << 24;
			val |= static_cast<int64_t>(buff[5]) << 16;
			val |= static_cast<int64_t>(buff[6]) << 8;
			val |= static_cast<int64_t>(buff[7]);
			return true;
		}
	}
	return false;
}

const bool FileIO::ReadStr(std::string& val, const int len)
{
	if (m_file)
	{
		std::vector<uint8_t> buff(len, 0);
		size_t rlen = fread(&buff[0], 1, buff.size(), m_file);
		if (rlen == len)
		{
			val.clear();
			val = std::string(buff.begin(), buff.end());
			return true;
		}
	}
	return false;
}

void FileIO::WriteInt8(const int8_t val)
{
	if (m_file)
	{
		fwrite(&val, 1, 1, m_file);
	}
}

void FileIO::WriteUInt8(const uint8_t val)
{
	if (m_file)
	{
		fwrite(&val, 1, 1, m_file);
	}
}

void FileIO::WriteInt32(const int32_t val)
{
	if (m_file)
	{
		std::vector<uint8_t> buff(4, 0);
		buff[0] = (val >> 24) & 0xff;
		buff[1] = (val >> 16) & 0xff;
		buff[2] = (val >> 8) & 0xff;
		buff[3] = (val) & 0xff;
		fwrite(&buff[0], 1, buff.size(), m_file);
	}
}

void FileIO::WriteInt64(const int64_t val)
{
	if (m_file)
	{
		std::vector<uint8_t> buff(8, 0);
		buff[0] = (val >> 56) & 0xff;
		buff[1] = (val >> 48) & 0xff;
		buff[2] = (val >> 40) & 0xff;
		buff[3] = (val >> 32) & 0xff;
		buff[4] = (val >> 24) & 0xff;
		buff[5] = (val >> 16) & 0xff;
		buff[6] = (val >> 8) & 0xff;
		buff[7] = (val) & 0xff;
		fwrite(&buff[0], 1, buff.size(), m_file);
	}
}

void FileIO::WriteStr(const std::string& val)
{
	if (m_file)
	{
		std::vector<uint8_t> buff(val.begin(), val.end());
		if (buff.size() > 0)
		{
			fwrite(&buff[0], 1, buff.size(), m_file);
		}
	}
}
