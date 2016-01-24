#include "Shared\Binary Buffer\BinaryBuffer.hpp"
#include <iterator>

namespace Utility
{
	BinaryBuffer::BinaryBuffer()
	{

	}

	BinaryBuffer::BinaryBuffer(size_t reserversize)
	{
		Reserve(reserversize);
	}

	BinaryBuffer::BinaryBuffer(const void* data, size_t length)
	{
		Append(data, length);
	}

	BinaryBuffer::BinaryBuffer(BinaryBuffer&& other)
	{
		SetData(std::move(other.Data));
	}

	BinaryBuffer::BinaryBuffer(ContainerType<ContainerDataType>&& data)
	{
		Append(std::move(data));
	}

	BinaryBuffer::BinaryBuffer(std::ifstream& fileinputstream) :
		InputStream(&fileinputstream)
	{
		
	}

	Utility::BinaryBuffer BinaryBuffer::operator=(BinaryBuffer&& other)
	{
		return {std::move(other)};
	}

	void BinaryBuffer::Reserve(size_t size)
	{
		Data.reserve(size);
	}

	void BinaryBuffer::Resize(size_t newsize)
	{
		Data.resize(newsize);
	}

	void BinaryBuffer::SetData(BinaryBuffer::ContainerType<ContainerDataType>&& data)
	{
		ReadPos = 0;
		Data = std::move(data);
	}

	void BinaryBuffer::Append(BinaryBuffer&& other)
	{
		if (Data.empty())
		{
			Data = std::move(other.Data);
		}

		else
		{
			Data.reserve(Data.size() + other.Data.size());
			std::move(other.Data.begin(), other.Data.end(), std::back_inserter(Data));
			other.Data.clear();
		}
	}

	void BinaryBuffer::Append(ContainerType<ContainerDataType>&& data)
	{
		if (Data.empty())
		{
			Data = std::move(data);
		}

		else
		{
			Data.reserve(Data.size() + data.size());
			std::move(data.begin(), data.end(), std::back_inserter(Data));
			data.clear();
		}
	}

	void BinaryBuffer::Append(const void* data, size_t length)
	{
		size_t insertpos = Data.size();
		Data.resize(insertpos + length);
		std::memcpy(&Data[insertpos], data, length);
	}

	void BinaryBuffer::SaveToFile(std::ofstream& outputstream)
	{
		outputstream.write(static_cast<const char*>(GetData()), GetSize());
	}

	BinaryBuffer::ContainerType<BinaryBuffer::ContainerDataType>&& BinaryBuffer::MoveData()
	{
		ReadPos = 0;
		return std::move(Data);
	}

	void BinaryBuffer::Clear()
	{
		Data.clear();
		ReadPos = 0;
	}

	const void* BinaryBuffer::GetData() const
	{
		if (Data.empty())
		{
			return nullptr;
		}

		return Data.data();
	}

	void* BinaryBuffer::GetDataModify()
	{
		return Data.data();
	}

	size_t BinaryBuffer::GetSize() const
	{
		return Data.size();
	}

	void BinaryBuffer::SetReadPosition(size_t position)
	{
		if (position >= Data.size())
		{
			position = Data.size();
		}

		ReadPos = position;
	}

	void BinaryBuffer::MoveReadPosition(int offset)
	{
		if (Data.empty())
		{
			return;
		}

		if (ReadPos + offset < 0)
		{
			ReadPos = 0;
			return;
		}

		if (ReadPos + offset > Data.size())
		{
			ReadPos = Data.size();
			return;
		}

		ReadPos += offset;
	}

	size_t BinaryBuffer::GetReadPosition() const
	{
		return ReadPos;
	}

	BinaryBuffer& BinaryBuffer::operator>>(bool& data)
	{
		uint8_t value;
		*this >> value;

		data = value != 0;

		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator>>(char& data)
	{
		if (InputStream)
		{
			InputStream->read(reinterpret_cast<char*>(&data), sizeof(data));
			return *this;
		}

		if (CanRead(sizeof(data)))
		{
			GenericRead(data);
		}

		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator>>(unsigned char& data)
	{
		if (InputStream)
		{
			InputStream->read(reinterpret_cast<char*>(&data), sizeof(data));
			return *this;
		}

		if (CanRead(sizeof(data)))
		{
			GenericRead(data);
		}

		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator>>(int16_t& data)
	{
		if (InputStream)
		{
			InputStream->read(reinterpret_cast<char*>(&data), sizeof(data));
			return *this;
		}

		if (CanRead(sizeof(data)))
		{
			GenericRead(data);
		}

		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator>>(uint16_t& data)
	{
		if (InputStream)
		{
			InputStream->read(reinterpret_cast<char*>(&data), sizeof(data));
			return *this;
		}

		if (CanRead(sizeof(data)))
		{
			GenericRead(data);
		}

		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator>>(int32_t& data)
	{
		if (InputStream)
		{
			InputStream->read(reinterpret_cast<char*>(&data), sizeof(data));
			return *this;
		}

		if (CanRead(sizeof(data)))
		{
			GenericRead(data);
		}

		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator>>(uint32_t& data)
	{
		if (InputStream)
		{
			InputStream->read(reinterpret_cast<char*>(&data), sizeof(data));
			return *this;
		}

		if (CanRead(sizeof(data)))
		{
			GenericRead(data);
		}

		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator>>(int64_t& data)
	{
		if (InputStream)
		{
			InputStream->read(reinterpret_cast<char*>(&data), sizeof(data));
			return *this;
		}

		if (CanRead(sizeof(data)))
		{
			GenericRead(data);
		}

		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator>>(uint64_t& data)
	{
		if (InputStream)
		{
			InputStream->read(reinterpret_cast<char*>(&data), sizeof(data));
			return *this;
		}

		if (CanRead(sizeof(data)))
		{
			GenericRead(data);
		}

		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator>>(float& data)
	{
		if (InputStream)
		{
			InputStream->read(reinterpret_cast<char*>(&data), sizeof(data));
			return *this;
		}

		if (CanRead(sizeof(data)))
		{
			GenericRead(data);
		}

		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator>>(double& data)
	{
		if (InputStream)
		{
			InputStream->read(reinterpret_cast<char*>(&data), sizeof(data));
			return *this;
		}

		if (CanRead(sizeof(data)))
		{
			GenericRead(data);
		}

		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator>>(std::string& data)
	{
		StringLengthType length;
		*this >> length;

		if (length == 0)
		{
			return *this;
		}

		if (InputStream)
		{
			data.resize(length);
			InputStream->read(&data[0], length);

			return *this;
		}

		if (CanRead(length))
		{
			data.assign(reinterpret_cast<char*>(&Data[ReadPos]), length);
			ReadPos += length;
		}

		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator>>(std::wstring& data)
	{
		std::string str;
		*this >> str;

		data = Utility::UTF8ToWString(std::move(str));

		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator<<(bool data)
	{
		*this << static_cast<uint8_t>(data);
		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator<<(char data)
	{
		GenericWrite(data);
		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator<<(unsigned char data)
	{
		GenericWrite(data);
		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator<<(int16_t data)
	{
		GenericWrite(data);
		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator<<(uint16_t data)
	{
		GenericWrite(data);
		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator<<(int32_t data)
	{
		GenericWrite(data);
		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator<<(uint32_t data)
	{
		GenericWrite(data);
		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator<<(int64_t data)
	{
		GenericWrite(data);
		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator<<(uint64_t data)
	{
		GenericWrite(data);
		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator<<(float data)
	{
		GenericWrite(data);
		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator<<(double data)
	{
		GenericWrite(data);
		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator<<(const std::string& data)
	{
		StringLengthType length = static_cast<StringLengthType>(data.size());
		*this << length;

		if (length > 0)
		{
			Append(data.c_str(), length);
		}

		return *this;
	}

	BinaryBuffer& BinaryBuffer::operator<<(const std::wstring& data)
	{
		auto str = Utility::WStringToUTF8(data.c_str());

		*this << str;

		return *this;
	}

	bool BinaryBuffer::CanRead(size_t size)
	{
		return (ReadPos + size <= Data.size());
	}
}
