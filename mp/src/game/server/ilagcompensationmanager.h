//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ILAGCOMPENSATIONMANAGER_H
#define ILAGCOMPENSATIONMANAGER_H
#ifdef _WIN32
#pragma once
#endif

class CBasePlayer;
class CUserCmd;

#ifdef NEWLAGCOMP
enum LagCompensationType
{
	LAG_COMPENSATE_BOUNDS,
	LAG_COMPENSATE_HITBOXES,
	LAG_COMPENSATE_HITBOXES_ALONG_RAY,
};
#endif
//-----------------------------------------------------------------------------
// Purpose: This is also an IServerSystem
//-----------------------------------------------------------------------------
abstract_class ILagCompensationManager
{
public:
#ifdef NEWLAGCOMP
	virtual void	StartLagCompensation (
		CBasePlayer * player,
		LagCompensationType lagCompensationType,
		const Vector & weaponPos = vec3_origin,
		const QAngle & weaponAngles = vec3_angle,
		float weaponRange = 0.0f ) = 0;
	virtual void	FinishLagCompensation ( CBasePlayer* player ) = 0;

	// Mappers can flag certain additional entities to lag compensate, this handles them
	virtual void	AddAdditionalEntity ( CBaseEntity* pEntity ) = 0;
	virtual void	RemoveAdditionalEntity ( CBaseEntity* pEntity ) = 0;
#else
	virtual void	StartLagCompensation(CBasePlayer *player, CUserCmd *cmd) = 0;
	virtual void	FinishLagCompensation(CBasePlayer *player) = 0;
	virtual bool	IsCurrentlyDoingLagCompensation() const = 0;
#endif
};

extern ILagCompensationManager *lagcompensation;

#endif // ILAGCOMPENSATIONMANAGER_H
