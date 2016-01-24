#pragma once
#include <string>
#include <vector>
#include <tuple>
#include "Shared\String\String.hpp"
#include <stdint.h>
#include <fstream>

namespace Utility
{
	/*
		Local binary data buffer, not for use over network.
	*/
	class BinaryBuffer final
	{
	public:
		using ContainerDataType = unsigned char;
		using StringLengthType = uint16_t;

		template <typename T>
		using ContainerType = std::vector<T>;

		template <typename T>
		static BinaryBuffer CreatePacket(T value)
		{
			return BinaryBuffer(&value, sizeof(value));
		}

		BinaryBuffer();
		BinaryBuffer(size_t reserversize);
		BinaryBuffer(const void* data, size_t length);
		BinaryBuffer(BinaryBuffer&& other);
		BinaryBuffer(ContainerType<ContainerDataType>&& data);
		BinaryBuffer(std::ifstream& fileinputstream);

		BinaryBuffer operator=(BinaryBuffer&& other);

		void Reserve(size_t size);
		void Resize(size_t newsize);

		void SetData(ContainerType<ContainerDataType>&& data);

		void Append(BinaryBuffer&& other);
		void Append(ContainerType<ContainerDataType>&& data);
		void Append(const void* data, size_t length);

		void SaveToFile(std::ofstream& outputstream);

		ContainerType<ContainerDataType>&& MoveData();

		void Clear();

		const void* GetData() const;
		void* GetDataModify();
		size_t GetSize() const;

		void SetReadPosition(size_t position);
		void MoveReadPosition(int offset);
		size_t GetReadPosition() const;

		BinaryBuffer& operator<<(bool data);
		BinaryBuffer& operator<<(char data);
		BinaryBuffer& operator<<(unsigned char data);
		BinaryBuffer& operator<<(int16_t data);
		BinaryBuffer& operator<<(uint16_t data);
		BinaryBuffer& operator<<(int32_t data);
		BinaryBuffer& operator<<(uint32_t data);
		BinaryBuffer& operator<<(int64_t data);
		BinaryBuffer& operator<<(uint64_t data);
		BinaryBuffer& operator<<(float data);
		BinaryBuffer& operator<<(double data);
		BinaryBuffer& operator<<(const std::string& data);
		BinaryBuffer& operator<<(const std::wstring& data);

		BinaryBuffer& operator>>(bool& data);
		BinaryBuffer& operator>>(char& data);
		BinaryBuffer& operator>>(unsigned char& data);
		BinaryBuffer& operator>>(int16_t& data);
		BinaryBuffer& operator>>(uint16_t& data);
		BinaryBuffer& operator>>(int32_t& data);
		BinaryBuffer& operator>>(uint32_t& data);
		BinaryBuffer& operator>>(int64_t& data);
		BinaryBuffer& operator>>(uint64_t& data);
		BinaryBuffer& operator>>(float& data);
		BinaryBuffer& operator>>(double& data);
		BinaryBuffer& operator>>(std::string& data);
		BinaryBuffer& operator>>(std::wstring& data);

		template <typename T>
		inline T GetValue()
		{
			T ret;
			*this >> ret;
			return ret;
		}

		inline std::wstring GetString()
		{
			return GetValue<std::wstring>();
		}

		inline std::string GetNormalString()
		{
			return GetValue<std::string>();
		}

	private:
		bool CanRead(size_t size);
		size_t ReadPos = 0;
		ContainerType<ContainerDataType> Data;

		std::ifstream* InputStream = nullptr;

		template <typename T>
		inline BinaryBuffer& GenericWrite(T data)
		{
			Append(&data, sizeof(data));
			return *this;
		}

		template <typename T>
		inline void GenericRead(T& data)
		{
			data = (*reinterpret_cast
			<
				std::add_const
				<
					std::add_pointer
					<
						std::remove_reference<decltype(data)>::type
					>::type
				>::type
			>(&Data[ReadPos]));
			
			ReadPos += sizeof(data);
		}
	};

	namespace BinaryBufferPrivate
	{
		template
		<
			size_t Index = 0,
			typename... Args,
			size_t Size = sizeof...(Args)
		>
		inline typename std::enable_if<Index == Size>::type
		AppendPack(BinaryBuffer& pack, std::tuple<Args...>&& tuple)
		{

		}

		template
		<
			size_t Index = 0,
			typename... Args,
			size_t Size = sizeof...(Args)
		>
		inline typename std::enable_if<Index != Size>::type
		AppendPack(BinaryBuffer& pack, std::tuple<Args...>&& tuple)
		{
			pack << std::get<Index>(tuple);
			AppendPack<Index + 1>(pack, std::move(tuple));
		}
	}

	namespace BinaryBufferHelp
	{
		template <typename... Args>
		inline BinaryBuffer CreatePacket(const Args&... args)
		{
			BinaryBuffer pack;

			BinaryBufferPrivate::AppendPack(pack, std::make_tuple(args...));

			return pack;
		}
	}
}
