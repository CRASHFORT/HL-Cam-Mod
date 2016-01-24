#include <string>
#include <codecvt>
#include "Shared\String\String.hpp"

namespace
{
	static std::wstring_convert<std::codecvt_utf8<wchar_t>> Converter;
}

std::string Utility::WStringToUTF8(const wchar_t* str)
{
	return Converter.to_bytes(str);
}

std::string Utility::WStringToUTF8(const std::wstring& str)
{
	return Converter.to_bytes(str);
}

std::wstring Utility::UTF8ToWString(const char* str)
{
	return Converter.from_bytes(str);
}

std::wstring Utility::UTF8ToWString(const std::string& str)
{
	return Converter.from_bytes(str);
}

bool Utility::CompareString(const char* first, const char* other)
{
	return strcmp(first, other) == 0;
}

bool Utility::CompareString(const wchar_t* first, const wchar_t* other)
{
	return wcscmp(first, other) == 0;
}
