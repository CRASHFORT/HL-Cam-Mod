#pragma once
#include <vector>
#include "Shared\Shared.hpp"

namespace Cam
{
	void Init();
	void VidInit();
	
	void OnUpdate();

	bool InEditMode();

	/*
		Required data to render a client
		preview of what the server has.
	*/

	struct ClientCamera
	{
		size_t ID;
		std::vector<size_t> LinkedTriggerIDs;

		/*
			A named camera will have its name
			drawn under it.
		*/
		bool IsNamed;
		char Name[64];

		float Position[3] = {0};
		float Angle[3] = {0};

		bool Selected = false;

		bool Adjusting = false;

		bool InPreview = false;
	};

	struct ClientTrigger
	{
		size_t ID;
		size_t LinkedCameraID;

		float Corner1[3] = {0};
		float Corner2[3] = {0};

		bool Highlighted = false;
		bool Selected = false;
	};

	const std::vector<ClientTrigger>& GetAllTriggers();
	const std::vector<ClientCamera>& GetAllCameras();

	void GetActiveCameraPosition(float* outpos);
}
