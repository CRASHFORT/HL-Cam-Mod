#pragma once

namespace Cam
{
	/*
		Required data to render a client
		preview of what the server has.
	*/

	struct ClientCamera
	{
		size_t ID;
		size_t LinkedTriggerID;

		/*
			A named camera will have its name
			drawn under it.
		*/
		bool IsNamed;
		char Name[64];

		float Position[3] = {0};
		float Angle[3] = {0};
	};

	struct ClientTrigger
	{
		size_t ID;
		size_t LinkedCameraID;

		float Corner1[3] = {0};
		float Corner2[3] = {0};
	};
}
