#pragma once

#ifdef CAM_EXTERN
#define CAM_PREFIX extern
#define CAM_SUFFIX
#else
#define CAM_PREFIX
#define CAM_SUFFIX = 0
#endif

#define AddMessage(name) CAM_PREFIX int name CAM_SUFFIX

namespace HLCamMessage
{
	/*
		SHORT: ID
		BYTE: Is named
		COORD x 3: Position XYZ
		COORD x 3: Angle XYZ

		->	Is named:
		|	STRING: Camera name

		If this is not a named camera, MsgHLCAM_OnCreateTrigger
		will be sent after this to make the player create a
		linked trigger.
	*/
	AddMessage(CreateCamera);

	/*	
		BYTE: Part
	
		->	Part == 0:
		|	SHORT: ID
		|	SHORT: Camera ID
		->	Part == 1:
		|	COORD x 3: Corner 1 XYZ
		->	Part == 2:
		|	COORD x 3: Corner 2 XYZ
	*/
	AddMessage(CreateTrigger);

	/*
		SHORT: ID

		If a trigger is linked to this camera
		it will be removed too.
	*/
	AddMessage(RemoveCamera);

	/*
		SHORT: ID
	*/
	AddMessage(RemoveTrigger);

	/*
		BYTE: On or off

		Player will go third person when edit mode stops,
		first person when edit mode starts.
	*/
	AddMessage(MapEditStateChanged);

	/*
		Message to reset client stuff
	*/
	AddMessage(MapReset);

	/*
		BYTE: 0 for trigger, 1 for camera
		SHORT: Item ID
	*/
	AddMessage(ItemHighlightedStart);

	/*
		Previous item is not highlighted anymore
	*/
	AddMessage(ItemHighlightedEnd);

	/*
		BYTE: 0 for trigger, 1 for camera
		SHORT: Item ID
	*/
	AddMessage(ItemSelectedStart);

	/*
		Previous item is not selected anymore
	*/
	AddMessage(ItemSelectedEnd);

	/*
		BYTE: State: 0 or 1

		->	State == 0:
		|	SHORT: Camera ID
		->	State == 1:
		|	SHORT: Camera ID
		|	COORD x 3: New position
		|	COORD x 3: New angle
	*/
	AddMessage(CameraAdjust);

	/*
		SHORT: Camera ID
		BYTE: On or off
	*/
	AddMessage(CameraPreview);

	/*
		COORD x 3: New camera position
	*/
	AddMessage(CameraSwitch);
}

#undef AddMessage
#undef CAM_PREFIX
#undef CAM_SUFFIX
