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

Cam::Shared::CameraAngleType Cam::Shared::CameraAngleTypeFromString(const wchar_t* string)
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

Cam::Shared::CameraAngleType Cam::Shared::CameraAngleTypeFromString(const char* string)
{
	if (Utility::CompareString(string, "Linear"))
	{
		return CameraAngleType::Linear;
	}

	else if (Utility::CompareString(string, "Smooth"))
	{
		return CameraAngleType::Smooth;
	}

	else if (Utility::CompareString(string, "Exponential"))
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

Cam::Shared::CameraLookType Cam::Shared::CameraLookTypeFromString(const wchar_t* string)
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

Cam::Shared::CameraLookType Cam::Shared::CameraLookTypeFromString(const char* string)
{
	if (Utility::CompareString(string, "At player"))
	{
		return CameraLookType::AtPlayer;
	}

	else if (Utility::CompareString(string, "At angle"))
	{
		return CameraLookType::AtAngle;
	}

	else if (Utility::CompareString(string, "At target"))
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

Cam::Shared::CameraPlaneType Cam::Shared::CameraPlaneTypeFromString(const wchar_t* string)
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

Cam::Shared::CameraPlaneType Cam::Shared::CameraPlaneTypeFromString(const char* string)
{
	if (Utility::CompareString(string, "Horizontal"))
	{
		return CameraPlaneType::Horizontal;
	}

	else if (Utility::CompareString(string, "Vertical"))
	{
		return CameraPlaneType::Vertical;
	}

	else if (Utility::CompareString(string, "Both"))
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

Cam::Shared::CameraTriggerType Cam::Shared::CameraTriggerTypeFromString(const wchar_t* string)
{
	if (Utility::CompareString(string, L"By name"))
	{
		return CameraTriggerType::ByName;
	}

	else if (Utility::CompareString(string, L"By user trigger"))
	{
		return CameraTriggerType::ByUserTrigger;
	}

	return CameraTriggerType::ByUserTrigger;
}

Cam::Shared::CameraTriggerType Cam::Shared::CameraTriggerTypeFromString(const char* string)
{
	if (Utility::CompareString(string, "By name"))
	{
		return CameraTriggerType::ByName;
	}

	else if (Utility::CompareString(string, "By user trigger"))
	{
		return CameraTriggerType::ByUserTrigger;
	}

	return CameraTriggerType::ByUserTrigger;
}

const char* Cam::Shared::CameraZoomTypeToString(CameraZoomType type)
{
	switch (type)
	{
		case Cam::Shared::CameraZoomType::None:
		{
			return "None";
		}

		case Cam::Shared::CameraZoomType::ZoomIn:
		{
			return "Zoom in";
		}

		case Cam::Shared::CameraZoomType::ZoomOut:
		{
			return "Zoom out";
		}

		case Cam::Shared::CameraZoomType::ZoomByDistance:
		{
			return "Zoom by distance";
		}
	}

	return nullptr;
}

Cam::Shared::CameraZoomType Cam::Shared::CameraZoomTypeFromString(const wchar_t* string)
{
	if (Utility::CompareString(string, L"None"))
	{
		return CameraZoomType::None;
	}

	else if (Utility::CompareString(string, L"Zoom in"))
	{
		return CameraZoomType::ZoomIn;
	}

	else if (Utility::CompareString(string, L"Zoom out"))
	{
		return CameraZoomType::ZoomOut;
	}

	else if (Utility::CompareString(string, L"Zoom by distance"))
	{
		return CameraZoomType::ZoomByDistance;
	}

	return CameraZoomType::None;
}

Cam::Shared::CameraZoomType Cam::Shared::CameraZoomTypeFromString(const char* string)
{
	if (Utility::CompareString(string, "None"))
	{
		return CameraZoomType::None;
	}

	if (Utility::CompareString(string, "Zoom in"))
	{
		return CameraZoomType::ZoomIn;
	}

	else if (Utility::CompareString(string, "Zoom out"))
	{
		return CameraZoomType::ZoomOut;
	}

	else if (Utility::CompareString(string, "Zoom by distance"))
	{
		return CameraZoomType::ZoomByDistance;
	}

	return CameraZoomType::None;
}
