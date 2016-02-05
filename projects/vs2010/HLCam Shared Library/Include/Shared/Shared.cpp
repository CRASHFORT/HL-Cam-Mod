#include "Shared.hpp"
#include <string>
#include "Shared\String\String.hpp"

const char* Cam::Shared::CameraAngleTypeToString(CameraAngleType type)
{
	switch (type)
	{
		case Cam::Shared::CameraAngleType::Linear:
		{
			return "Linear";
		}

		case Cam::Shared::CameraAngleType::Smooth:
		{
			return "Smooth";
		}

		case Cam::Shared::CameraAngleType::Exponential:
		{
			return "Exponential";
		}
	}

	return nullptr;
}

Cam::Shared::CameraAngleType Cam::Shared::CameraAngleTypeFromStringWide(const wchar_t* string)
{
	if (Utility::CompareString(string, L"Linear"))
	{
		return CameraAngleType::Linear;
	}

	else if (Utility::CompareString(string, L"Smooth"))
	{
		return CameraAngleType::Smooth;
	}

	else if (Utility::CompareString(string, L"Exponential"))
	{
		return CameraAngleType::Exponential;
	}

	return CameraAngleType::Linear;
}

const char* Cam::Shared::CameraLookTypeToString(CameraLookType type)
{
	switch (type)
	{
		case Cam::Shared::CameraLookType::AtPlayer:
		{
			return "At player";
		}

		case Cam::Shared::CameraLookType::AtAngle:
		{
			return "At angle";
		}

		case Cam::Shared::CameraLookType::AtTarget:
		{
			return "At target";
		}
	}

	return nullptr;
}

Cam::Shared::CameraLookType Cam::Shared::CameraLookTypeFromStringWide(const wchar_t* string)
{
	if (Utility::CompareString(string, L"At player"))
	{
		return CameraLookType::AtPlayer;
	}

	else if (Utility::CompareString(string, L"At angle"))
	{
		return CameraLookType::AtAngle;
	}

	else if (Utility::CompareString(string, L"At target"))
	{
		return CameraLookType::AtTarget;
	}

	return CameraLookType::AtPlayer;
}

const char* Cam::Shared::CameraPlaneTypeToString(CameraPlaneType type)
{
	switch (type)
	{
		case Cam::Shared::CameraPlaneType::Horizontal:
		{
			return "Horizontal";
		}

		case Cam::Shared::CameraPlaneType::Vertical:
		{
			return "Vertical";
		}

		case Cam::Shared::CameraPlaneType::Both:
		{
			return "Both";
		}
	}

	return nullptr;
}

Cam::Shared::CameraPlaneType Cam::Shared::CameraPlaneTypeFromStringWide(const wchar_t* string)
{
	if (Utility::CompareString(string, L"Horizontal"))
	{
		return CameraPlaneType::Horizontal;
	}

	else if (Utility::CompareString(string, L"Vertical"))
	{
		return CameraPlaneType::Vertical;
	}

	else if (Utility::CompareString(string, L"Both"))
	{
		return CameraPlaneType::Both;
	}

	return CameraPlaneType::Horizontal;
}

const char* Cam::Shared::CameraTriggerTypeToString(CameraTriggerType type)
{
	switch (type)
	{
		case Cam::Shared::CameraTriggerType::ByName:
		{
			return "By name";
		}

		case Cam::Shared::CameraTriggerType::ByUserTrigger:
		{
			return "By user trigger";
		}
	}

	return nullptr;
}

Cam::Shared::CameraTriggerType Cam::Shared::CameraTriggerTypeFromStringWide(const wchar_t* string)
{
	if (Utility::CompareString(string, L"By name"))
	{
		return CameraTriggerType::ByName;
	}

	else if (Utility::CompareString(string, L"By user trigger"))
	{
		return CameraTriggerType::ByName;
	}

	return CameraTriggerType::ByName;
}
