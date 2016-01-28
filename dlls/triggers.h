#pragma once
#include "HLCam Server\Server.hpp"

#define SF_CAMERA_PLAYER_POSITION	1
#define SF_CAMERA_PLAYER_TARGET		2
#define SF_CAMERA_PLAYER_TAKECONTROL 4

class CTriggerCamera : public CBaseDelay
{
public:
	void Spawn(void);
	void KeyValue(KeyValueData *pkvd);
	void Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
	void EXPORT FollowTarget(void);
	void Move(void);

	virtual int		Save(CSave &save);
	virtual int		Restore(CRestore &restore);
	virtual int	ObjectCaps(void)
	{
		return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION;
	}
	static	TYPEDESCRIPTION m_SaveData[];

	EHANDLE m_hPlayer;
	EHANDLE m_hTarget;
	CBaseEntity *m_pentPath;
	int	  m_sPath;
	float m_flWait;
	float m_flReturnTime;

	/*
		CRASH FORT:
	*/	
	bool IsHLCam = false;
	Cam::MapCamera HLCam;

	void SetFov(int fov);
	void SetPlayerFOV(int fov);

	float m_flStopTime;
	float m_moveDistance;
	float m_targetSpeed;
	float m_initialSpeed;
	float m_acceleration;
	float m_deceleration;
	int	  m_state;

};
