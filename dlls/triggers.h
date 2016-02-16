#pragma once
#include "HLCam Server\Server.hpp"

#define SF_CAMERA_PLAYER_POSITION	1
#define SF_CAMERA_PLAYER_TARGET		2
#define SF_CAMERA_PLAYER_TAKECONTROL 4

class CTriggerCamera : public CBaseDelay
{
public:
	virtual void Spawn() override;	
	virtual void Use(CBaseEntity* activator, CBaseEntity* caller, USE_TYPE usetype, float value) override;
	
	void EXPORT CameraThink();
	
	void ZoomAddon();
	
	virtual int	ObjectCaps() override
	{
		return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION | FCAP_DONT_SAVE;
	}

	EHANDLE PlayerHandle;
	EHANDLE TargetHandle;

	/*
		CRASH FORT:
	*/
	Cam::MapCamera HLCam;

	void SetupHLCamera(const Cam::MapCamera& camera);

	void SetFov(float fov);
	void SetPlayerFOV(float fov);

	float CurrentZoomFOV;
	float StartZoomTime;
	bool ReachedEndZoom;

	EHANDLE AttachmentEntity;
};
