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
	
	void EXPORT FollowTarget();

	virtual int Save(CSave &save) override;
	virtual int Restore(CRestore &restore) override;
	
	virtual int	ObjectCaps() override
	{
		return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION;
	}

	static TYPEDESCRIPTION m_SaveData[];

	EHANDLE PlayerHandle;
	EHANDLE TargetHandle;

	/*
		CRASH FORT:
	*/
	Cam::MapCamera HLCam;

	void SetupHLCamera(const Cam::MapCamera& camera);

	void SetFov(int fov);
	void SetPlayerFOV(int fov);
};
