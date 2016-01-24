#pragma once

namespace Utility
{
	std::string WStringToUTF8(const wchar_t* str);
	std::string WStringToUTF8(const std::wstring& str);

	std::wstring UTF8ToWString(const char* str);
	std::wstring UTF8ToWString(const std::string& str);

	bool CompareString(const char* first, const char* other);
	bool CompareString(const wchar_t* first, const wchar_t* other);
}
