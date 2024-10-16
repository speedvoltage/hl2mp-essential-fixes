﻿//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#ifdef NEWLAGCOMP

#include "tier0/platform.h"
#include "player_lagcompensation.h"
#include "usercmd.h"
#include "inetchannelinfo.h"
#include "utllinkedlist.h"
#include "BaseAnimatingOverlay.h"
#include "tier0/vprof.h"
#include "collisionutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define LC_NONE				0
#define LC_ALIVE			(1<<0)

#define LC_ORIGIN_CHANGED	(1<<8)
#define LC_ANGLES_CHANGED	(1<<9)
#define LC_SIZE_CHANGED		(1<<10)
#define LC_ANIMATION_CHANGED (1<<11)

#define LAG_COMPENSATION_TELEPORTED_DISTANCE_SQR ( 64.0f * 64.0f )
#define LAG_COMPENSATION_EPS_SQR ( 0.1f * 0.1f )
// Allow 4 units of error ( about 1 / 8 bbox width )
#define LAG_COMPENSATION_ERROR_EPS_SQR ( 4.0f * 4.0f )

ConVar sv_unlag ( "sv_unlag", "1", FCVAR_DEVELOPMENTONLY, "Enables player lag compensation" );
ConVar sv_maxunlag ( "sv_maxunlag", "1.0", FCVAR_DEVELOPMENTONLY, "Maximum lag compensation in seconds", true, 0.0f, true, 1.0f );
ConVar sv_lagflushbonecache ( "sv_lagflushbonecache", "1", FCVAR_DEVELOPMENTONLY, "Flushes entity bone cache on lag compensation" );
ConVar sv_showlagcompensation ( "sv_showlagcompensation", "0", FCVAR_CHEAT, "Show lag compensated hitboxes whenever a player is lag compensated." );
ConVar sv_showlagcompensation_duration ( "sv_showlagcompensation_duration", "4.0", FCVAR_CHEAT, "Duration to show lag-compensated hitboxes", true, 0.0f, true, 10.0f );
ConVar sv_lagcompensationforcerestore ( "sv_lagcompensationforcerestore", "1", FCVAR_CHEAT, "Don't test validity of a lag comp restore, just do it." );

ConVar sv_unlag_fixstuck ( "sv_unlag_fixstuck", "0", FCVAR_DEVELOPMENTONLY, "Disallow backtracking a player for lag compensation if it will cause them to become stuck" );

ConVar sv_lagpushticks ( "sv_lagpushticks", "0", FCVAR_DEVELOPMENTONLY, "Push computed lag compensation amount by this many ticks." );

ConVar sv_lagcompensateself ( "sv_lagcompensateself", "0", FCVAR_CHEAT, "Player can lag compensate themselves." );

#define COSINE_20F 0.93969f
#define SINE_20F 0.34202f

static CLagCompensationManager g_LagCompensationManager ( "CLagCompensationManager" );
ILagCompensationManager* lagcompensation = &g_LagCompensationManager;

//
// Try to take the player from his current origin to vWantedPos.
// If it can't get there, leave the player where he is.
// 

ConVar sv_unlag_debug ( "sv_unlag_debug", "0", FCVAR_GAMEDLL | FCVAR_DEVELOPMENTONLY );

float g_flFractionScale = 0.95;


static void LC_SetAbsOrigin ( CBaseEntity* entity, const Vector& vecAbsOrigin, bool bFireTriggers )
{
	entity->SetAbsOrigin ( vecAbsOrigin );
	if ( bFireTriggers )
	{
		entity->PhysicsTouchTriggers ( );
	}
}

static void RestoreEntityTo ( CBaseEntity* pEntity, const Vector& vWantedPos )
{
	// Try to move to the wanted position from our current position.
	trace_t tr;
	VPROF_BUDGET ( "RestoreEntityTo", "CLagCompensationManager" );
	if ( sv_lagcompensationforcerestore.GetBool ( ) )
	{
		// We don't have to test for validity.  Just put them back where you found them.
		LC_SetAbsOrigin ( pEntity, vWantedPos, true );
	}

	unsigned int mask = MASK_PLAYERSOLID;
	UTIL_TraceEntity ( pEntity, vWantedPos, vWantedPos, mask, pEntity, COLLISION_GROUP_PLAYER_MOVEMENT, &tr );
	if ( tr.startsolid || tr.allsolid )
	{
		if ( sv_unlag_debug.GetBool ( ) )
		{
			DevMsg ( "RestoreEntityTo could not restore player position for client \"%d\" ( %.1f %.1f %.1f )\n",
				pEntity->entindex ( ), vWantedPos.x, vWantedPos.y, vWantedPos.z );
		}

		UTIL_TraceEntity ( pEntity, pEntity->GetAbsOrigin ( ), vWantedPos, mask, pEntity, COLLISION_GROUP_PLAYER_MOVEMENT, &tr );
		if ( tr.startsolid || tr.allsolid )
		{
			// In this case, the guy got stuck back wherever we lag compensated him to. Nasty.

			if ( sv_unlag_debug.GetBool ( ) )
				DevMsg ( " restore failed entirely\n" );
		}
		else
		{
			// We can get to a valid place, but not all the way back to where we were.
			Vector vPos;
			VectorLerp ( pEntity->GetAbsOrigin ( ), vWantedPos, tr.fraction * g_flFractionScale, vPos );
			LC_SetAbsOrigin ( pEntity, vPos, true );

			if ( sv_unlag_debug.GetBool ( ) )
				DevMsg ( " restore got most of the way\n" );
		}
	}
	else
	{
		// Cool, the player can go back to whence he came.
		LC_SetAbsOrigin ( pEntity, tr.endpos, true );
	}
}

// Mappers can flag certain additional entities to lag compensate, this handles them
void CLagCompensationManager::AddAdditionalEntity ( CBaseEntity* pEntity )
{
	EHANDLE eh;
	eh = pEntity;
	if ( m_AdditionalEntities.Find ( eh ) == m_AdditionalEntities.InvalidIndex ( ) )
	{
		m_AdditionalEntities.Insert ( eh );
	}
}

void CLagCompensationManager::RemoveAdditionalEntity ( CBaseEntity* pEntity )
{
	EHANDLE eh;
	eh = pEntity;
	m_AdditionalEntities.Remove ( eh );
}

//-----------------------------------------------------------------------------
// Purpose: Called once per frame after all entities have had a chance to think
//-----------------------------------------------------------------------------
void CLagCompensationManager::FrameUpdatePostEntityThink ( )
{
	if ( ( gpGlobals->maxClients <= 1 ) || !sv_unlag.GetBool ( ) )
	{
		ClearHistory ( );
		return;
	}

	VPROF_BUDGET ( "FrameUpdatePostEntityThink", "CLagCompensationManager" );

	CUtlRBTree< CBaseEntity* > rbEntityList ( 0, 0, DefLessFunc ( CBaseEntity* ) );

	// Add active players
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex ( i );
		if ( !pPlayer )
		{
			continue;
		}

		if ( rbEntityList.Find ( pPlayer ) == rbEntityList.InvalidIndex ( ) )
		{
			rbEntityList.Insert ( pPlayer );
		}
	}

	// Add any additional entities
	for ( int i = m_AdditionalEntities.FirstInorder ( ); i != m_AdditionalEntities.InvalidIndex ( ); i = m_AdditionalEntities.NextInorder ( i ) )
	{
		CBaseEntity* pAddEntity = m_AdditionalEntities [ i ].Get ( );
		if ( !pAddEntity )
			continue;

		if ( rbEntityList.Find ( pAddEntity ) == rbEntityList.InvalidIndex ( ) )
		{
			rbEntityList.Insert ( pAddEntity );
		}
	}

	// Now record the actual history information
	for ( int i = rbEntityList.FirstInorder ( ); i != rbEntityList.InvalidIndex ( ); i = rbEntityList.NextInorder ( i ) )
	{
		CBaseEntity* pEntity = rbEntityList [ i ];

		EHANDLE eh;
		eh = pEntity;

		int slot = m_CompensatedEntities.Find ( eh );
		if ( slot == m_CompensatedEntities.InvalidIndex ( ) )
		{
			EntityLagData* pNewEntry = new EntityLagData ( );
			slot = m_CompensatedEntities.Insert ( eh, pNewEntry );
		}

		EntityLagData* ld = m_CompensatedEntities [ slot ];

		RecordDataIntoTrack ( pEntity, &ld->m_LagRecords, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called during gamemovment weapon firing to set up/restore after lag compensation around the bullet traces
//-----------------------------------------------------------------------------
void CLagCompensationManager::StartLagCompensation ( CBasePlayer* player, LagCompensationType lagCompensationType, const Vector& weaponPos, const QAngle& weaponAngles, float weaponRange )
{
	Assert ( !m_isCurrentlyDoingCompensation );

	// Assume no entities need to be restored
	CUtlVector< EHANDLE > invalidList;
	FOR_EACH_MAP ( m_CompensatedEntities, i )
	{
		EntityLagData* ld = m_CompensatedEntities [ i ];
		EHANDLE key;
		key = m_CompensatedEntities.Key ( i );
		if ( !key.Get ( ) )
		{
			// Note that the EHANDLE is NULL now
			invalidList.AddToTail ( key );
			continue;
		}

		// Clear state
		ld->m_bRestoreEntity = false;
		ld->m_RestoreData.Clear ( );
		ld->m_ChangeData.Clear ( );
	}

	// Wipe any deleted entities from the list
	for ( int i = 0; i < invalidList.Count ( ); ++i )
	{
		int slot = m_CompensatedEntities.Find ( invalidList [ i ] );
		Assert ( slot != m_CompensatedEntities.InvalidIndex ( ) );
		if ( slot == m_CompensatedEntities.InvalidIndex ( ) )
			continue;

		EntityLagData* ld = m_CompensatedEntities [ slot ];
		delete ld;
		m_CompensatedEntities.RemoveAt ( slot );
	}

	m_bNeedToRestore = false;

	m_pCurrentPlayer = player;

	if ( !player->m_bLagCompensation		// Player not wanting lag compensation
		|| ( gpGlobals->maxClients <= 1 )	// no lag compensation in single player
		|| !sv_unlag.GetBool ( )				// disabled by server admin
		|| player->IsBot ( ) 				// not for bots
		|| player->IsObserver ( )			// not for spectators
		)
		return;

	const CUserCmd* cmd = player->GetCurrentUserCommand ( );
	if ( !cmd )
	{
		// This can hit if m_hActiveWeapon incorrectly gets set to a weapon not actually owned by the local player 
		// (since lag comp asks for the GetPlayerOwner() which will have m_pCurrentCommand == NULL, 
		//  not the player currently executing a CUserCmd).
		Error ( "CLagCompensationManager::StartLagCompensation with NULL CUserCmd!!!\n" );
	}

	// NOTE: Put this here so that it won't show up in single player mode.
	VPROF_BUDGET ( "StartLagCompensation", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	m_isCurrentlyDoingCompensation = true;

	// correct is the amount of time we have to correct game time
	float correct = 0.0f;

	// Get true latency
	INetChannelInfo* nci = engine->GetPlayerNetInfo ( player->entindex ( ) );
	if ( nci )
	{
		// add network latency
		correct += nci->GetLatency ( FLOW_OUTGOING );
	}

	// NOTE:  do these computations in float time, not ticks, to avoid big roundoff error accumulations in the math
	// add view interpolation latency see C_BaseEntity::GetInterpolationAmount()
	correct += player->m_fLerpTime;

	// check bounds [0,sv_maxunlag]
	correct = clamp ( correct, 0.0f, sv_maxunlag.GetFloat ( ) );

	// correct tick send by player 
	float flTargetTime = TICKS_TO_TIME ( cmd->tick_count ) - player->m_fLerpTime;

	// calculate difference between tick sent by player and our latency based tick
	float deltaTime = correct - ( gpGlobals->curtime - flTargetTime );

	if ( fabs ( deltaTime ) > 0.2f )
	{
		// difference between cmd time and latency is too big > 200ms, use time correction based on latency
		// DevMsg("StartLagCompensation: delta too big (%.3f)\n", deltaTime );
		flTargetTime = gpGlobals->curtime - correct;
	}

	flTargetTime += TICKS_TO_TIME ( sv_lagpushticks.GetInt ( ) );

	m_lagCompensationType = lagCompensationType;
	m_weaponPos = weaponPos;
	m_weaponAngles = weaponAngles;
	m_weaponRange = weaponRange;

	// populate position, angles and range if the provided values are zeroed
	if ( m_weaponPos == vec3_origin )
	{
		m_weaponPos = m_pCurrentPlayer->EyePosition ( );
	}

	if ( m_weaponAngles == vec3_angle )
	{
		m_weaponAngles = m_pCurrentPlayer->EyeAngles ( );
	}

	if ( m_weaponRange <= 0 )
	{
		m_weaponRange = FLOAT32_MAX;
	}

	// Iterate all lag compensatable entities
	const CBitVec<MAX_EDICTS>* pEntityTransmitBits = engine->GetEntityTransmitBitsForClient ( player->entindex ( ) - 1 );

	FOR_EACH_MAP ( m_CompensatedEntities, i )
	{
		EntityLagData* ld = m_CompensatedEntities [ i ];
		EHANDLE key = m_CompensatedEntities.Key ( i );
		CBaseEntity* pEntity = key.Get ( );
		if ( !pEntity )
		{
			// Stale entity in list, fixed up on next time through loop
			continue;
		}

		// Don't lag compensate self, unless we've set a convar explicitly allowing it
		if ( player == pEntity && !sv_lagcompensateself.GetBool ( ) )
		{
			continue;
		}

		// Custom checks for if things should lag compensate (based on things like what team the entity is associated with).
		const CBasePlayer* pPlayer = dynamic_cast< const CBasePlayer* >( pEntity );
		if ( pPlayer && !player->WantsLagCompensationOnEntity ( pPlayer, cmd, pEntityTransmitBits ) )
			continue;

		// Move entity back in time and remember that fact
		ld->m_bRestoreEntity = BacktrackEntity ( pEntity, flTargetTime, &ld->m_LagRecords, &ld->m_RestoreData, &ld->m_ChangeData, true );
	}
}

bool CLagCompensationManager::BacktrackEntity ( CBaseEntity* entity, float flTargetTime, LagRecordList* track, LagRecord* restore, LagRecord* change, bool wantsAnims )
{
	Vector org, mins, maxs;
	QAngle ang;

	VPROF_BUDGET ( "BacktrackEntity", "CLagCompensationManager" );

	// check if we have at least one entry
	if ( track->Count ( ) <= 0 )
		return false;

	intp curr = track->Head ( );

	LagRecord* prevRecord = NULL;
	LagRecord* record = NULL;

	Vector prevOrg = entity->GetAbsOrigin ( );

	// Walk context looking for any invalidating event
	while ( track->IsValidIndex ( curr ) )
	{
		// remember last record
		prevRecord = record;

		// get next record
		record = &track->Element ( curr );

		if ( !( record->m_fFlags & LC_ALIVE ) )
		{
			// entity must be alive, lost track
			return false;
		}

		Vector delta = record->m_vecOrigin - prevOrg;
		if ( delta.LengthSqr ( ) > LAG_COMPENSATION_TELEPORTED_DISTANCE_SQR )
		{
			// lost track, too much difference
			return false;
		}

		// did we find a context smaller than target time ?
		if ( record->m_flSimulationTime <= flTargetTime )
			break; // hurra, stop

		prevOrg = record->m_vecOrigin;

		// go one step back
		curr = track->Next ( curr );
	}

	Assert ( record );

	if ( !record )
	{
		if ( sv_unlag_debug.GetBool ( ) )
		{
			DevMsg ( "No valid positions in history for BacktrackPlayer client ( %d )\n", entity->entindex ( ) );
		}
		return false;
	}

	float frac = 0.0f;
	if ( prevRecord &&
		( record->m_flSimulationTime < flTargetTime ) &&
		( record->m_flSimulationTime < prevRecord->m_flSimulationTime ) )
	{
		// we didn't find the exact time but have a valid previous record
		// so interpolate between these two records;

		Assert ( prevRecord->m_flSimulationTime > record->m_flSimulationTime );
		Assert ( flTargetTime < prevRecord->m_flSimulationTime );

		// calc fraction between both records
		frac = ( flTargetTime - record->m_flSimulationTime ) /
			( prevRecord->m_flSimulationTime - record->m_flSimulationTime );

		Assert ( frac > 0 && frac < 1 ); // should never extrapolate

		ang = Lerp ( frac, record->m_vecAngles, prevRecord->m_vecAngles );
		org = Lerp ( frac, record->m_vecOrigin, prevRecord->m_vecOrigin );
		mins = Lerp ( frac, record->m_vecMins, prevRecord->m_vecMins );
		maxs = Lerp ( frac, record->m_vecMaxs, prevRecord->m_vecMaxs );
	}
	else
	{
		// we found the exact record or no other record to interpolate with
		// just copy these values since they are the best we have
		ang = record->m_vecAngles;
		org = record->m_vecOrigin;
		mins = record->m_vecMins;
		maxs = record->m_vecMaxs;
	}

	// See if this is still a valid position for us to teleport to
	if ( sv_unlag_fixstuck.GetBool ( ) )
	{
		// Try to move to the wanted position from our current position.
		trace_t tr;
		UTIL_TraceEntity ( entity, org, org, MASK_PLAYERSOLID, &tr );
		if ( tr.startsolid || tr.allsolid )
		{
			if ( sv_unlag_debug.GetBool ( ) )
				DevMsg ( "WARNING: BackupPlayer trying to back player into a bad position - client %d\n", entity->entindex ( ) );

			CBaseEntity* pHitEntity = tr.m_pEnt;
			// don't lag compensate the current player
			if ( pHitEntity && ( pHitEntity != m_pCurrentPlayer ) )
			{
				// Find it
				EHANDLE eh;
				eh = pHitEntity;

				int slot = m_CompensatedEntities.Find ( eh );
				if ( slot != m_CompensatedEntities.InvalidIndex ( ) )
				{
					EntityLagData* ld = m_CompensatedEntities [ slot ];

					// If we haven't backtracked this player, do it now
					// this deliberately ignores WantsLagCompensationOnEntity.
					if ( !ld->m_bRestoreEntity )
					{
						// Temp turn this flag on
						ld->m_bRestoreEntity = true;

						BacktrackEntity ( pHitEntity, flTargetTime, &ld->m_LagRecords, &ld->m_RestoreData, &ld->m_ChangeData, true );

						// Remove the temp flag
						ld->m_bRestoreEntity = false;
					}
				}
			}

			// now trace us back as far as we can go
			unsigned int mask = MASK_PLAYERSOLID;
			UTIL_TraceEntity ( entity, entity->GetAbsOrigin ( ), org, mask, &tr );

			if ( tr.startsolid || tr.allsolid )
			{
				// Our starting position is bogus

				if ( sv_unlag_debug.GetBool ( ) )
					DevMsg ( "Backtrack failed completely, bad starting position\n" );
			}
			else
			{
				// We can get to a valid place, but not all the way to the target
				Vector vPos;
				VectorLerp ( entity->GetAbsOrigin ( ), org, tr.fraction * g_flFractionScale, vPos );

				// This is as close as we're going to get
				org = vPos;

				if ( sv_unlag_debug.GetBool ( ) )
					DevMsg ( "Backtrack got most of the way\n" );
			}
		}
	}

	// See if this represents a change for the entity
	int flags = 0;

	QAngle angdiff = entity->GetAbsAngles ( ) - ang;
	Vector orgdiff = entity->GetAbsOrigin ( ) - org;

	// Always remember the pristine simulation time in case we need to restore it.
	restore->m_flSimulationTime = entity->GetSimulationTime ( );

	if ( angdiff.LengthSqr ( ) > LAG_COMPENSATION_EPS_SQR )
	{
		flags |= LC_ANGLES_CHANGED;
		restore->m_vecAngles = entity->GetAbsAngles ( );
		entity->SetAbsAngles ( ang );
		change->m_vecAngles = ang;
	}

	// Use absolute equality here
	if ( ( mins != entity->WorldAlignMins ( ) ) ||
		( maxs != entity->WorldAlignMaxs ( ) ) )
	{
		flags |= LC_SIZE_CHANGED;
		restore->m_vecMins = entity->WorldAlignMins ( );
		restore->m_vecMaxs = entity->WorldAlignMaxs ( );
		entity->SetSize ( mins, maxs );
		change->m_vecMins = mins;
		change->m_vecMaxs = maxs;
	}

	// Note, do origin at end since it causes a relink into the k/d tree
	if ( orgdiff.LengthSqr ( ) > LAG_COMPENSATION_EPS_SQR )
	{
		flags |= LC_ORIGIN_CHANGED;
		restore->m_vecOrigin = entity->GetAbsOrigin ( );
		entity->SetAbsOrigin ( org );
		change->m_vecOrigin = org;
	}

	bool skipAnims = false;
	switch ( m_lagCompensationType )
	{
	case LAG_COMPENSATE_BOUNDS:
		skipAnims = true;
		break;
	case LAG_COMPENSATE_HITBOXES:
		skipAnims = false;
		break;
	case LAG_COMPENSATE_HITBOXES_ALONG_RAY:
		skipAnims = false;
		{
			// don't bother to set up lag-compensated animation layers on entities that fall outside a 20-degree cone from the weapon firing point
			Vector vecWeaponForward;
			AngleVectors ( m_weaponAngles, &vecWeaponForward );
			if ( !IsSphereIntersectingCone ( entity->WorldSpaceCenter ( ), entity->BoundingRadius ( ) + 10.0f, m_weaponPos, vecWeaponForward, SINE_20F, COSINE_20F ) )
			{
				skipAnims = true;

				//indicate ignored entities with a red box
				if ( sv_showlagcompensation.GetInt ( ) == 1 && entity->GetBaseAnimating ( ) )
					debugoverlay->AddBoxOverlay ( entity->WorldSpaceCenter ( ), Vector ( -3, -3, -3 ), Vector ( 3, 3, 3 ), QAngle ( 0, 0, 0 ), 255, 0, 0, 255, sv_showlagcompensation_duration.GetFloat ( ) * 0.125f );
			}
		}
		break;
	}

	if ( !skipAnims && wantsAnims && entity->GetBaseAnimating ( ) )
	{
		CBaseAnimating* pAnimating = entity->GetBaseAnimating ( );

		// Sorry for the loss of the optimization for the case of people
		// standing still, but you breathe even on the server.
		// This is quicker than actually comparing all bazillion floats.
		flags |= LC_ANIMATION_CHANGED;
		restore->m_masterSequence = pAnimating->GetSequence ( );
		restore->m_masterCycle = pAnimating->GetCycle ( );

		pAnimating->SetSequence ( record->m_masterSequence );
		pAnimating->SetCycle ( record->m_masterCycle );

		// populate restore record's pose parameters
		CStudioHdr* pHdr = pAnimating->GetModelPtr ( );
		if ( pHdr )
		{
			for ( int i = 0; i < pHdr->GetNumPoseParameters ( ); i++ )
			{
				restore->m_flPoseParameters [ i ] = pAnimating->GetPoseParameter ( i );

				// apply the record's pose parameter
				pAnimating->SetPoseParameter ( i, record->m_flPoseParameters [ i ] );
			}
		}

		////////////////////////
		// Now do all the layers
		//
		CBaseAnimatingOverlay* pAnimatingOverlay = entity->GetBaseAnimatingOverlay ( );
		if ( pAnimatingOverlay )
		{
			int layerCount = pAnimatingOverlay->GetNumAnimOverlays ( );
			for ( int layerIndex = 0; layerIndex < layerCount; ++layerIndex )
			{
				CAnimationLayer* currentLayer = pAnimatingOverlay->GetAnimOverlay ( layerIndex );
				if ( currentLayer )
				{
					restore->m_layerRecords [ layerIndex ].m_cycle = currentLayer->m_flCycle;
					restore->m_layerRecords [ layerIndex ].m_order = currentLayer->m_nOrder;
					restore->m_layerRecords [ layerIndex ].m_sequence = currentLayer->m_nSequence;
					restore->m_layerRecords [ layerIndex ].m_weight = currentLayer->m_flWeight;


					currentLayer->m_flCycle = record->m_layerRecords [ layerIndex ].m_cycle;
					currentLayer->m_nOrder = record->m_layerRecords [ layerIndex ].m_order;
					currentLayer->m_nSequence = record->m_layerRecords [ layerIndex ].m_sequence;
					currentLayer->m_flWeight = record->m_layerRecords [ layerIndex ].m_weight;

				}
			}
		}

		//indicate lag-compensated entities with a green box
		if ( sv_showlagcompensation.GetInt ( ) == 1 && entity->GetBaseAnimating ( ) )
			debugoverlay->AddBoxOverlay ( entity->WorldSpaceCenter ( ), Vector ( -3, -3, -3 ), Vector ( 3, 3, 3 ), QAngle ( 0, 0, 0 ), 0, 255, 0, 255, sv_showlagcompensation_duration.GetFloat ( ) * 0.125f );

	}

	if ( !flags )
		return false; // we didn't change anything

	if ( sv_lagflushbonecache.GetBool ( ) && ( flags & LC_ANIMATION_CHANGED ) && entity->GetBaseAnimating ( ) )
		entity->GetBaseAnimating ( )->InvalidateBoneCache ( );

	/*
	char text[256]; Q_snprintf( text, sizeof(text), "time %.2f", flTargetTime );
	pPlayer->DrawServerHitboxes( 10 );
	NDebugOverlay::Text( org, text, false, 10 );
	if ( skipAnims )
	{
	NDebugOverlay::EntityBounds( pPlayer, 0, 255, 0, 32, 10 );
	}
	else
	{
	NDebugOverlay::EntityBounds( pPlayer, 255, 0, 0, 32, 10 );
	}
	*/

	m_bNeedToRestore = true;  // we changed at least one entity
	restore->m_fFlags = flags; // we need to restore these flags
	change->m_fFlags = flags; // we have changed these flags

	if ( sv_showlagcompensation.GetInt ( ) == 1 && entity->GetBaseAnimating ( ) )
	{
		entity->GetBaseAnimating ( )->DrawServerHitboxes ( sv_showlagcompensation_duration.GetFloat ( ), true );
	}

	return true;// Yes, this guy has been backtracked
}


void CLagCompensationManager::FinishLagCompensation ( CBasePlayer* player )
{
	VPROF_BUDGET_FLAGS ( "FinishLagCompensation", VPROF_BUDGETGROUP_OTHER_NETWORKING, BUDGETFLAG_CLIENT | BUDGETFLAG_SERVER );

	m_isCurrentlyDoingCompensation = false;

	if ( !m_bNeedToRestore )
		return; // no entity was changed at all

	// Iterate all active entities
	FOR_EACH_MAP ( m_CompensatedEntities, i )
	{
		EntityLagData* ld = m_CompensatedEntities [ i ];
		// entity wasn't changed by lag compensation
		if ( !ld->m_bRestoreEntity )
			continue;

		CBaseEntity* pEntity = m_CompensatedEntities.Key ( i ).Get ( );
		if ( !pEntity )
			continue;

		LagRecord* restore = &ld->m_RestoreData;
		LagRecord* change = &ld->m_ChangeData;

		RestoreEntityFromRecords ( pEntity, restore, change, true );
	}
}

void CLagCompensationManager::RecordDataIntoTrack ( CBaseEntity* entity, LagRecordList* track, bool wantsAnims )
{
	Assert ( track->Count ( ) < 1000 ); // insanity check

	// remove all records before that time:
	int flDeadtime = gpGlobals->curtime - sv_maxunlag.GetFloat ( );

	// remove tail records that are too old
	intp tailIndex = track->Tail ( );
	while ( track->IsValidIndex ( tailIndex ) )
	{
		LagRecord& tail = track->Element ( tailIndex );

		// if tail is within limits, stop
		if ( tail.m_flSimulationTime >= flDeadtime )
			break;

		// remove tail, get new tail
		track->Remove ( tailIndex );
		tailIndex = track->Tail ( );
	}

	// check if head has same simulation time
	if ( track->Count ( ) > 0 )
	{
		LagRecord& head = track->Element ( track->Head ( ) );

		// check if player changed simulation time since last time updated
		if ( head.m_flSimulationTime >= entity->GetSimulationTime ( ) )
			return; // don't add new entry for same or older time
	}

	// add new record to entity track
	LagRecord& record = track->Element ( track->AddToHead ( ) );

	record.m_fFlags = 0;
	if ( entity->IsAlive ( ) )
	{
		record.m_fFlags |= LC_ALIVE;
	}

	record.m_flSimulationTime = entity->GetSimulationTime ( );
	record.m_vecAngles = entity->GetAbsAngles ( );
	record.m_vecOrigin = entity->GetAbsOrigin ( );
	record.m_vecMaxs = entity->WorldAlignMaxs ( );
	record.m_vecMins = entity->WorldAlignMins ( );

	CBaseAnimating* pAnimating = entity->GetBaseAnimating ( );

	if ( wantsAnims && pAnimating )
	{
		CBaseAnimatingOverlay* pAnimatingOverlay = entity->GetBaseAnimatingOverlay ( );

		if ( pAnimatingOverlay )
		{
			int layerCount = pAnimatingOverlay->GetNumAnimOverlays ( );
			for ( int layerIndex = 0; layerIndex < layerCount; ++layerIndex )
			{
				CAnimationLayer* currentLayer = pAnimatingOverlay->GetAnimOverlay ( layerIndex );
				if ( currentLayer )
				{
					record.m_layerRecords [ layerIndex ].m_cycle = currentLayer->m_flCycle;
					record.m_layerRecords [ layerIndex ].m_order = currentLayer->m_nOrder;
					record.m_layerRecords [ layerIndex ].m_sequence = currentLayer->m_nSequence;
					record.m_layerRecords [ layerIndex ].m_weight = currentLayer->m_flWeight;
				}
			}
		}
		record.m_masterSequence = pAnimating->GetSequence ( );
		record.m_masterCycle = pAnimating->GetCycle ( );

		CStudioHdr* pHdr = pAnimating->GetModelPtr ( );
		if ( pHdr )
		{
			for ( int i = 0; i < pHdr->GetNumPoseParameters ( ); i++ )
			{
				record.m_flPoseParameters [ i ] = pAnimating->GetPoseParameter ( i );
			}
		}
	}
}
void CLagCompensationManager::RestoreEntityFromRecords ( CBaseEntity* entity, LagRecord* restore, LagRecord* change, bool wantsAnims )
{
	bool restoreSimulationTime = false;

	if ( restore->m_fFlags & LC_SIZE_CHANGED )
	{
		restoreSimulationTime = true;

		// see if simulation made any changes, if no, then do the restore, otherwise,
		//  leave new values in
		if ( entity->WorldAlignMins ( ) == change->m_vecMins &&
			entity->WorldAlignMaxs ( ) == change->m_vecMaxs )
		{
			// Restore it
			entity->SetSize ( restore->m_vecMins, restore->m_vecMaxs );
		}
	}

	if ( restore->m_fFlags & LC_ANGLES_CHANGED )
	{
		restoreSimulationTime = true;

		if ( entity->GetAbsAngles ( ) == change->m_vecAngles )
		{
			entity->SetAbsAngles ( restore->m_vecAngles );
		}
	}

	if ( restore->m_fFlags & LC_ORIGIN_CHANGED )
	{
		restoreSimulationTime = true;

		// Okay, let's see if we can do something reasonable with the change
		Vector delta = entity->GetAbsOrigin ( ) - change->m_vecOrigin;

		// If it moved really far, just leave the entity in the new spot!!!
		if ( delta.LengthSqr ( ) < LAG_COMPENSATION_TELEPORTED_DISTANCE_SQR )
		{
			RestoreEntityTo ( entity, restore->m_vecOrigin + delta );
		}
	}

	CBaseAnimating* pAnimating = entity->GetBaseAnimating ( );

	if ( wantsAnims && pAnimating )
	{
		if ( restore->m_fFlags & LC_ANIMATION_CHANGED )
		{
			restoreSimulationTime = true;

			pAnimating->SetSequence ( restore->m_masterSequence );
			pAnimating->SetCycle ( restore->m_masterCycle );

			CBaseAnimatingOverlay* pAnimatingOverlay = entity->GetBaseAnimatingOverlay ( );
			if ( pAnimatingOverlay )
			{
				int layerCount = pAnimatingOverlay->GetNumAnimOverlays ( );
				for ( int layerIndex = 0; layerIndex < layerCount; ++layerIndex )
				{
					CAnimationLayer* currentLayer = pAnimatingOverlay->GetAnimOverlay ( layerIndex );
					if ( currentLayer )
					{
						currentLayer->m_flCycle = restore->m_layerRecords [ layerIndex ].m_cycle;
						currentLayer->m_nOrder = restore->m_layerRecords [ layerIndex ].m_order;
						currentLayer->m_nSequence = restore->m_layerRecords [ layerIndex ].m_sequence;
						currentLayer->m_flWeight = restore->m_layerRecords [ layerIndex ].m_weight;
					}
				}
			}

			CStudioHdr* pHdr = pAnimating->GetModelPtr ( );
			if ( pHdr )
			{
				for ( int i = 0; i < pHdr->GetNumPoseParameters ( ); i++ )
				{
					pAnimating->SetPoseParameter ( i, restore->m_flPoseParameters [ i ] );
				}
			}
		}
	}

	if ( restoreSimulationTime )
	{
		entity->SetSimulationTime ( restore->m_flSimulationTime );
	}
}
#else
#include "usercmd.h"
#include "igamesystem.h"
#include "ilagcompensationmanager.h"
#include "inetchannelinfo.h"
#include "utllinkedlist.h"
#include "BaseAnimatingOverlay.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define LC_NONE				0
#define LC_ALIVE			(1<<0)

#define LC_ORIGIN_CHANGED	(1<<8)
#define LC_ANGLES_CHANGED	(1<<9)
#define LC_SIZE_CHANGED		(1<<10)
#define LC_ANIMATION_CHANGED (1<<11)

static ConVar sv_lagcompensation_teleport_dist("sv_lagcompensation_teleport_dist", "64", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT, "How far a player got moved by game code before we can't lag compensate their position back");
#define LAG_COMPENSATION_EPS_SQR ( 0.1f * 0.1f )
// Allow 4 units of error ( about 1 / 8 bbox width )
#define LAG_COMPENSATION_ERROR_EPS_SQR ( 4.0f * 4.0f )

ConVar sv_unlag("sv_unlag", "1", FCVAR_DEVELOPMENTONLY, "Enables player lag compensation");
ConVar sv_maxunlag("sv_maxunlag", "1.0", FCVAR_DEVELOPMENTONLY, "Maximum lag compensation in seconds", true, 0.0f, true, 1.0f);
ConVar sv_lagflushbonecache("sv_lagflushbonecache", "1", FCVAR_DEVELOPMENTONLY, "Flushes entity bone cache on lag compensation");
ConVar sv_showlagcompensation("sv_showlagcompensation", "0", FCVAR_CHEAT, "Show lag compensated hitboxes whenever a player is lag compensated.");

ConVar sv_unlag_fixstuck("sv_unlag_fixstuck", "0", FCVAR_DEVELOPMENTONLY, "Disallow backtracking a player for lag compensation if it will cause them to become stuck");

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
#define MAX_LAYER_RECORDS (CBaseAnimatingOverlay::MAX_OVERLAYS)

struct LayerRecord
{
	int m_sequence;
	float m_cycle;
	float m_weight;
	int m_order;

	LayerRecord()
	{
		m_sequence = 0;
		m_cycle = 0;
		m_weight = 0;
		m_order = 0;
	}

	LayerRecord(const LayerRecord& src)
	{
		m_sequence = src.m_sequence;
		m_cycle = src.m_cycle;
		m_weight = src.m_weight;
		m_order = src.m_order;
	}
};

struct LagRecord
{
public:
	LagRecord()
	{
		m_fFlags = 0;
		m_vecOrigin.Init();
		m_vecAngles.Init();
		m_vecMinsPreScaled.Init();
		m_vecMaxsPreScaled.Init();
		m_flSimulationTime = -1;
		m_masterSequence = 0;
		m_masterCycle = 0;
	}

	LagRecord(const LagRecord& src)
	{
		m_fFlags = src.m_fFlags;
		m_vecOrigin = src.m_vecOrigin;
		m_vecAngles = src.m_vecAngles;
		m_vecMinsPreScaled = src.m_vecMinsPreScaled;
		m_vecMaxsPreScaled = src.m_vecMaxsPreScaled;
		m_flSimulationTime = src.m_flSimulationTime;
		for (int layerIndex = 0; layerIndex < MAX_LAYER_RECORDS; ++layerIndex)
		{
			m_layerRecords[layerIndex] = src.m_layerRecords[layerIndex];
		}
		m_masterSequence = src.m_masterSequence;
		m_masterCycle = src.m_masterCycle;
	}

	// Did player die this frame
	int						m_fFlags;

	// Player position, orientation and bbox
	Vector					m_vecOrigin;
	QAngle					m_vecAngles;
	Vector					m_vecMinsPreScaled;
	Vector					m_vecMaxsPreScaled;

	float					m_flSimulationTime;

	// Player animation details, so we can get the legs in the right spot.
	LayerRecord				m_layerRecords[MAX_LAYER_RECORDS];
	int						m_masterSequence;
	float					m_masterCycle;
};


//
// Try to take the player from his current origin to vWantedPos.
// If it can't get there, leave the player where he is.
// 

ConVar sv_unlag_debug("sv_unlag_debug", "0", FCVAR_GAMEDLL | FCVAR_DEVELOPMENTONLY);

float g_flFractionScale = 0.95;
static void RestorePlayerTo(CBasePlayer *pPlayer, const Vector &vWantedPos)
{
	// Try to move to the wanted position from our current position.
	trace_t tr;
	VPROF_BUDGET("RestorePlayerTo", "CLagCompensationManager");
	UTIL_TraceEntity(pPlayer, vWantedPos, vWantedPos, MASK_PLAYERSOLID, pPlayer, COLLISION_GROUP_PLAYER_MOVEMENT, &tr);
	if (tr.startsolid || tr.allsolid)
	{
		if (sv_unlag_debug.GetBool())
		{
			DevMsg("RestorePlayerTo() could not restore player position for client \"%s\" ( %.1f %.1f %.1f )\n",
				pPlayer->GetPlayerName(), vWantedPos.x, vWantedPos.y, vWantedPos.z);
		}

		UTIL_TraceEntity(pPlayer, pPlayer->GetLocalOrigin(), vWantedPos, MASK_PLAYERSOLID, pPlayer, COLLISION_GROUP_PLAYER_MOVEMENT, &tr);
		if (tr.startsolid || tr.allsolid)
		{
			// In this case, the guy got stuck back wherever we lag compensated him to. Nasty.

			if (sv_unlag_debug.GetBool())
				DevMsg(" restore failed entirely\n");
		}
		else
		{
			// We can get to a valid place, but not all the way back to where we were.
			Vector vPos;
			VectorLerp(pPlayer->GetLocalOrigin(), vWantedPos, tr.fraction * g_flFractionScale, vPos);
			UTIL_SetOrigin(pPlayer, vPos, true);

			if (sv_unlag_debug.GetBool())
				DevMsg(" restore got most of the way\n");
		}
	}
	else
	{
		// Cool, the player can go back to whence he came.
		UTIL_SetOrigin(pPlayer, tr.endpos, true);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CLagCompensationManager : public CAutoGameSystemPerFrame, public ILagCompensationManager
{
public:
	CLagCompensationManager(char const *name) : CAutoGameSystemPerFrame(name), m_flTeleportDistanceSqr(64 * 64)
	{
		m_isCurrentlyDoingCompensation = false;
	}

	// IServerSystem stuff
	virtual void Shutdown()
	{
		ClearHistory();
	}

	virtual void LevelShutdownPostEntity()
	{
		ClearHistory();
	}

	// called after entities think
	virtual void FrameUpdatePostEntityThink();

	// ILagCompensationManager stuff

	// Called during player movement to set up/restore after lag compensation
	void			StartLagCompensation(CBasePlayer *player, CUserCmd *cmd);
	void			FinishLagCompensation(CBasePlayer *player);

	bool			IsCurrentlyDoingLagCompensation() const OVERRIDE{ return m_isCurrentlyDoingCompensation; }

private:
	void			BacktrackPlayer(CBasePlayer *player, float flTargetTime);

	void ClearHistory()
	{
		for (int i = 0; i<MAX_PLAYERS; i++)
			m_PlayerTrack[i].Purge();
	}

	// keep a list of lag records for each player
	CUtlFixedLinkedList< LagRecord >	m_PlayerTrack[MAX_PLAYERS];

	// Scratchpad for determining what needs to be restored
	CBitVec<MAX_PLAYERS>	m_RestorePlayer;
	bool					m_bNeedToRestore;

	LagRecord				m_RestoreData[MAX_PLAYERS];	// player data before we moved him back
	LagRecord				m_ChangeData[MAX_PLAYERS];	// player data where we moved him back

	CBasePlayer				*m_pCurrentPlayer;	// The player we are doing lag compensation for

	float					m_flTeleportDistanceSqr;

	bool					m_isCurrentlyDoingCompensation;	// Sentinel to prevent calling StartLagCompensation a second time before a Finish.
};

static CLagCompensationManager g_LagCompensationManager("CLagCompensationManager");
ILagCompensationManager *lagcompensation = &g_LagCompensationManager;


//-----------------------------------------------------------------------------
// Purpose: Called once per frame after all entities have had a chance to think
//-----------------------------------------------------------------------------
void CLagCompensationManager::FrameUpdatePostEntityThink()
{
	if ((gpGlobals->maxClients <= 1) || !sv_unlag.GetBool())
	{
		ClearHistory();
		return;
	}

	m_flTeleportDistanceSqr = sv_lagcompensation_teleport_dist.GetFloat() * sv_lagcompensation_teleport_dist.GetFloat();

	VPROF_BUDGET("FrameUpdatePostEntityThink", "CLagCompensationManager");

	// remove all records before that time:
	int flDeadtime = gpGlobals->curtime - sv_maxunlag.GetFloat();

	// Iterate all active players
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex(i);

		CUtlFixedLinkedList< LagRecord > *track = &m_PlayerTrack[i - 1];

		if (!pPlayer)
		{
			if (track->Count() > 0)
			{
				track->RemoveAll();
			}

			continue;
		}

		Assert(track->Count() < 1000); // insanity check

		// remove tail records that are too old
		int tailIndex = track->Tail();
		while (track->IsValidIndex(tailIndex))
		{
			LagRecord &tail = track->Element(tailIndex);

			// if tail is within limits, stop
			if (tail.m_flSimulationTime >= flDeadtime)
				break;

			// remove tail, get new tail
			track->Remove(tailIndex);
			tailIndex = track->Tail();
		}

		// check if head has same simulation time
		if (track->Count() > 0)
		{
			LagRecord &head = track->Element(track->Head());

			// check if player changed simulation time since last time updated
			if (head.m_flSimulationTime >= pPlayer->GetSimulationTime())
				continue; // don't add new entry for same or older time
		}

		// add new record to player track
		LagRecord &record = track->Element(track->AddToHead());

		record.m_fFlags = 0;
		if (pPlayer->IsAlive())
		{
			record.m_fFlags |= LC_ALIVE;
		}

		record.m_flSimulationTime = pPlayer->GetSimulationTime();
		record.m_vecAngles = pPlayer->GetLocalAngles();
		record.m_vecOrigin = pPlayer->GetLocalOrigin();
		record.m_vecMinsPreScaled = pPlayer->CollisionProp()->OBBMinsPreScaled();
		record.m_vecMaxsPreScaled = pPlayer->CollisionProp()->OBBMaxsPreScaled();

		int layerCount = pPlayer->GetNumAnimOverlays();
		for (int layerIndex = 0; layerIndex < layerCount; ++layerIndex)
		{
			CAnimationLayer *currentLayer = pPlayer->GetAnimOverlay(layerIndex);
			if (currentLayer)
			{
				record.m_layerRecords[layerIndex].m_cycle = currentLayer->m_flCycle;
				record.m_layerRecords[layerIndex].m_order = currentLayer->m_nOrder;
				record.m_layerRecords[layerIndex].m_sequence = currentLayer->m_nSequence;
				record.m_layerRecords[layerIndex].m_weight = currentLayer->m_flWeight;
			}
		}
		record.m_masterSequence = pPlayer->GetSequence();
		record.m_masterCycle = pPlayer->GetCycle();
	}

	//Clear the current player.
	m_pCurrentPlayer = NULL;
}

// Called during player movement to set up/restore after lag compensation
void CLagCompensationManager::StartLagCompensation(CBasePlayer *player, CUserCmd *cmd)
{
	Assert(!m_isCurrentlyDoingCompensation);

	//DONT LAG COMP AGAIN THIS FRAME IF THERES ALREADY ONE IN PROGRESS
	//IF YOU'RE HITTING THIS THEN IT MEANS THERES A CODE BUG
	if (m_pCurrentPlayer)
	{
		Assert(m_pCurrentPlayer == NULL);
		Warning("Trying to start a new lag compensation session while one is already active!\n");
		return;
	}

	// Assume no players need to be restored
	m_RestorePlayer.ClearAll();
	m_bNeedToRestore = false;

	m_pCurrentPlayer = player;

	if (!player->m_bLagCompensation		// Player not wanting lag compensation
		|| (gpGlobals->maxClients <= 1)	// no lag compensation in single player
		|| !sv_unlag.GetBool()				// disabled by server admin
		|| player->IsBot() 				// not for bots
		|| player->IsObserver()			// not for spectators
		)
		return;

	// NOTE: Put this here so that it won't show up in single player mode.
	VPROF_BUDGET("StartLagCompensation", VPROF_BUDGETGROUP_OTHER_NETWORKING);
	Q_memset(m_RestoreData, 0, sizeof(m_RestoreData));
	Q_memset(m_ChangeData, 0, sizeof(m_ChangeData));

	m_isCurrentlyDoingCompensation = true;

	// Get true latency

	// correct is the amout of time we have to correct game time
	float correct = 0.0f;

	INetChannelInfo *nci = engine->GetPlayerNetInfo(player->entindex());

	if (nci)
	{
		// add network latency
		correct += nci->GetLatency(FLOW_OUTGOING);
	}

	// calc number of view interpolation ticks - 1
	int lerpTicks = TIME_TO_TICKS(player->m_fLerpTime);

	// add view interpolation latency see C_BaseEntity::GetInterpolationAmount()
	correct += TICKS_TO_TIME(lerpTicks);

	// check bouns [0,sv_maxunlag]
	correct = clamp(correct, 0.0f, sv_maxunlag.GetFloat());

	// correct tick send by player 
	int targettick = cmd->tick_count - lerpTicks;

	// calc difference between tick send by player and our latency based tick
	float deltaTime = correct - TICKS_TO_TIME(gpGlobals->tickcount - targettick);

	if (fabs(deltaTime) > 0.2f)
	{
		// difference between cmd time and latency is too big > 200ms, use time correction based on latency
		// DevMsg("StartLagCompensation: delta too big (%.3f)\n", deltaTime );
		targettick = gpGlobals->tickcount - TIME_TO_TICKS(correct);
	}

	// Iterate all active players
	const CBitVec<MAX_EDICTS> *pEntityTransmitBits = engine->GetEntityTransmitBitsForClient(player->entindex() - 1);
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex(i);

		if (!pPlayer)
		{
			continue;
		}

		// Don't lag compensate yourself you loser...
		if (player == pPlayer)
		{
			continue;
		}

		// Custom checks for if things should lag compensate (based on things like what team the player is on).
		if (!player->WantsLagCompensationOnEntity(pPlayer, cmd, pEntityTransmitBits))
			continue;

		// Move other player back in time
		BacktrackPlayer(pPlayer, TICKS_TO_TIME(targettick));
	}
}

void CLagCompensationManager::BacktrackPlayer(CBasePlayer *pPlayer, float flTargetTime)
{
	Vector org;
	Vector minsPreScaled;
	Vector maxsPreScaled;
	QAngle ang;

	VPROF_BUDGET("BacktrackPlayer", "CLagCompensationManager");
	int pl_index = pPlayer->entindex() - 1;

	// get track history of this player
	CUtlFixedLinkedList< LagRecord > *track = &m_PlayerTrack[pl_index];

	// check if we have at leat one entry
	if (track->Count() <= 0)
		return;

	int curr = track->Head();

	LagRecord *prevRecord = NULL;
	LagRecord *record = NULL;

	Vector prevOrg = pPlayer->GetLocalOrigin();

	// Walk context looking for any invalidating event
	while (track->IsValidIndex(curr))
	{
		// remember last record
		prevRecord = record;

		// get next record
		record = &track->Element(curr);

		if (!(record->m_fFlags & LC_ALIVE))
		{
			// player most be alive, lost track
			return;
		}

		Vector delta = record->m_vecOrigin - prevOrg;
		if (delta.Length2DSqr() > m_flTeleportDistanceSqr)
		{
			// lost track, too much difference
			return;
		}

		// did we find a context smaller than target time ?
		if (record->m_flSimulationTime <= flTargetTime)
			break; // hurra, stop

		prevOrg = record->m_vecOrigin;

		// go one step back
		curr = track->Next(curr);
	}

	Assert(record);

	if (!record)
	{
		if (sv_unlag_debug.GetBool())
		{
			DevMsg("No valid positions in history for BacktrackPlayer client ( %s )\n", pPlayer->GetPlayerName());
		}

		return; // that should never happen
	}

	float frac = 0.0f;
	if (prevRecord &&
		(record->m_flSimulationTime < flTargetTime) &&
		(record->m_flSimulationTime < prevRecord->m_flSimulationTime))
	{
		// we didn't find the exact time but have a valid previous record
		// so interpolate between these two records;

		Assert(prevRecord->m_flSimulationTime > record->m_flSimulationTime);
		Assert(flTargetTime < prevRecord->m_flSimulationTime);

		// calc fraction between both records
		frac = (flTargetTime - record->m_flSimulationTime) /
			(prevRecord->m_flSimulationTime - record->m_flSimulationTime);

		Assert(frac > 0 && frac < 1); // should never extrapolate

		ang = Lerp(frac, record->m_vecAngles, prevRecord->m_vecAngles);
		org = Lerp(frac, record->m_vecOrigin, prevRecord->m_vecOrigin);
		minsPreScaled = Lerp(frac, record->m_vecMinsPreScaled, prevRecord->m_vecMinsPreScaled);
		maxsPreScaled = Lerp(frac, record->m_vecMaxsPreScaled, prevRecord->m_vecMaxsPreScaled);
	}
	else
	{
		// we found the exact record or no other record to interpolate with
		// just copy these values since they are the best we have
		org = record->m_vecOrigin;
		ang = record->m_vecAngles;
		minsPreScaled = record->m_vecMinsPreScaled;
		maxsPreScaled = record->m_vecMaxsPreScaled;
	}

	// See if this is still a valid position for us to teleport to
	if (sv_unlag_fixstuck.GetBool())
	{
		// Try to move to the wanted position from our current position.
		trace_t tr;
		UTIL_TraceEntity(pPlayer, org, org, MASK_PLAYERSOLID, &tr);
		if (tr.startsolid || tr.allsolid)
		{
			if (sv_unlag_debug.GetBool())
				DevMsg("WARNING: BackupPlayer trying to back player into a bad position - client %s\n", pPlayer->GetPlayerName());

			CBasePlayer *pHitPlayer = dynamic_cast<CBasePlayer *>(tr.m_pEnt);

			// don't lag compensate the current player
			if (pHitPlayer && (pHitPlayer != m_pCurrentPlayer))
			{
				// If we haven't backtracked this player, do it now
				// this deliberately ignores WantsLagCompensationOnEntity.
				if (!m_RestorePlayer.Get(pHitPlayer->entindex() - 1))
				{
					// prevent recursion - save a copy of m_RestorePlayer,
					// pretend that this player is off-limits
					int pl_index = pPlayer->entindex() - 1;

					// Temp turn this flag on
					m_RestorePlayer.Set(pl_index);

					BacktrackPlayer(pHitPlayer, flTargetTime);

					// Remove the temp flag
					m_RestorePlayer.Clear(pl_index);
				}
			}

			// now trace us back as far as we can go
			UTIL_TraceEntity(pPlayer, pPlayer->GetLocalOrigin(), org, MASK_PLAYERSOLID, &tr);

			if (tr.startsolid || tr.allsolid)
			{
				// Our starting position is bogus

				if (sv_unlag_debug.GetBool())
					DevMsg("Backtrack failed completely, bad starting position\n");
			}
			else
			{
				// We can get to a valid place, but not all the way to the target
				Vector vPos;
				VectorLerp(pPlayer->GetLocalOrigin(), org, tr.fraction * g_flFractionScale, vPos);

				// This is as close as we're going to get
				org = vPos;

				if (sv_unlag_debug.GetBool())
					DevMsg("Backtrack got most of the way\n");
			}
		}
	}

	// See if this represents a change for the player
	int flags = 0;
	LagRecord *restore = &m_RestoreData[pl_index];
	LagRecord *change = &m_ChangeData[pl_index];

	QAngle angdiff = pPlayer->GetLocalAngles() - ang;
	Vector orgdiff = pPlayer->GetLocalOrigin() - org;

	// Always remember the pristine simulation time in case we need to restore it.
	restore->m_flSimulationTime = pPlayer->GetSimulationTime();

	if (angdiff.LengthSqr() > LAG_COMPENSATION_EPS_SQR)
	{
		flags |= LC_ANGLES_CHANGED;
		restore->m_vecAngles = pPlayer->GetLocalAngles();
		pPlayer->SetLocalAngles(ang);
		change->m_vecAngles = ang;
	}

	// Use absolute equality here
	if (minsPreScaled != pPlayer->CollisionProp()->OBBMinsPreScaled() || maxsPreScaled != pPlayer->CollisionProp()->OBBMaxsPreScaled())
	{
		flags |= LC_SIZE_CHANGED;

		restore->m_vecMinsPreScaled = pPlayer->CollisionProp()->OBBMinsPreScaled();
		restore->m_vecMaxsPreScaled = pPlayer->CollisionProp()->OBBMaxsPreScaled();

		pPlayer->SetSize(minsPreScaled, maxsPreScaled);

		change->m_vecMinsPreScaled = minsPreScaled;
		change->m_vecMaxsPreScaled = maxsPreScaled;
	}

	// Note, do origin at end since it causes a relink into the k/d tree
	if (orgdiff.LengthSqr() > LAG_COMPENSATION_EPS_SQR)
	{
		flags |= LC_ORIGIN_CHANGED;
		restore->m_vecOrigin = pPlayer->GetLocalOrigin();
		pPlayer->SetLocalOrigin(org);
		change->m_vecOrigin = org;
	}

	// Sorry for the loss of the optimization for the case of people
	// standing still, but you breathe even on the server.
	// This is quicker than actually comparing all bazillion floats.
	flags |= LC_ANIMATION_CHANGED;
	restore->m_masterSequence = pPlayer->GetSequence();
	restore->m_masterCycle = pPlayer->GetCycle();

	bool interpolationAllowed = false;
	if (prevRecord && (record->m_masterSequence == prevRecord->m_masterSequence))
	{
		// If the master state changes, all layers will be invalid too, so don't interp (ya know, interp barely ever happens anyway)
		interpolationAllowed = true;
	}

	////////////////////////
	// First do the master settings
	bool interpolatedMasters = false;
	if (frac > 0.0f && interpolationAllowed)
	{
		interpolatedMasters = true;
		pPlayer->SetSequence(Lerp(frac, record->m_masterSequence, prevRecord->m_masterSequence));
		pPlayer->SetCycle(Lerp(frac, record->m_masterCycle, prevRecord->m_masterCycle));

		if (record->m_masterCycle > prevRecord->m_masterCycle)
		{
			// the older record is higher in frame than the newer, it must have wrapped around from 1 back to 0
			// add one to the newer so it is lerping from .9 to 1.1 instead of .9 to .1, for example.
			float newCycle = Lerp(frac, record->m_masterCycle, prevRecord->m_masterCycle + 1);
			pPlayer->SetCycle(newCycle < 1 ? newCycle : newCycle - 1);// and make sure .9 to 1.2 does not end up 1.05
		}
		else
		{
			pPlayer->SetCycle(Lerp(frac, record->m_masterCycle, prevRecord->m_masterCycle));
		}
	}
	if (!interpolatedMasters)
	{
		pPlayer->SetSequence(record->m_masterSequence);
		pPlayer->SetCycle(record->m_masterCycle);
	}

	////////////////////////
	// Now do all the layers
	int layerCount = pPlayer->GetNumAnimOverlays();
	for (int layerIndex = 0; layerIndex < layerCount; ++layerIndex)
	{
		CAnimationLayer *currentLayer = pPlayer->GetAnimOverlay(layerIndex);
		if (currentLayer)
		{
			restore->m_layerRecords[layerIndex].m_cycle = currentLayer->m_flCycle;
			restore->m_layerRecords[layerIndex].m_order = currentLayer->m_nOrder;
			restore->m_layerRecords[layerIndex].m_sequence = currentLayer->m_nSequence;
			restore->m_layerRecords[layerIndex].m_weight = currentLayer->m_flWeight;

			bool interpolated = false;
			if ((frac > 0.0f) && interpolationAllowed)
			{
				LayerRecord &recordsLayerRecord = record->m_layerRecords[layerIndex];
				LayerRecord &prevRecordsLayerRecord = prevRecord->m_layerRecords[layerIndex];
				if ((recordsLayerRecord.m_order == prevRecordsLayerRecord.m_order)
					&& (recordsLayerRecord.m_sequence == prevRecordsLayerRecord.m_sequence)
					)
				{
					// We can't interpolate across a sequence or order change
					interpolated = true;
					if (recordsLayerRecord.m_cycle > prevRecordsLayerRecord.m_cycle)
					{
						// the older record is higher in frame than the newer, it must have wrapped around from 1 back to 0
						// add one to the newer so it is lerping from .9 to 1.1 instead of .9 to .1, for example.
						float newCycle = Lerp(frac, recordsLayerRecord.m_cycle, prevRecordsLayerRecord.m_cycle + 1);
						currentLayer->m_flCycle = newCycle < 1 ? newCycle : newCycle - 1;// and make sure .9 to 1.2 does not end up 1.05
					}
					else
					{
						currentLayer->m_flCycle = Lerp(frac, recordsLayerRecord.m_cycle, prevRecordsLayerRecord.m_cycle);
					}
					currentLayer->m_nOrder = recordsLayerRecord.m_order;
					currentLayer->m_nSequence = recordsLayerRecord.m_sequence;
					currentLayer->m_flWeight = Lerp(frac, recordsLayerRecord.m_weight, prevRecordsLayerRecord.m_weight);
				}
			}
			if (!interpolated)
			{
				//Either no interp, or interp failed.  Just use record.
				currentLayer->m_flCycle = record->m_layerRecords[layerIndex].m_cycle;
				currentLayer->m_nOrder = record->m_layerRecords[layerIndex].m_order;
				currentLayer->m_nSequence = record->m_layerRecords[layerIndex].m_sequence;
				currentLayer->m_flWeight = record->m_layerRecords[layerIndex].m_weight;
			}
		}
	}

	if (!flags)
		return; // we didn't change anything

	if (sv_lagflushbonecache.GetBool())
		pPlayer->InvalidateBoneCache();

	/*char text[256]; Q_snprintf( text, sizeof(text), "time %.2f", flTargetTime );
	pPlayer->DrawServerHitboxes( 10 );
	NDebugOverlay::Text( org, text, false, 10 );
	NDebugOverlay::EntityBounds( pPlayer, 255, 0, 0, 32, 10 ); */

	m_RestorePlayer.Set(pl_index); //remember that we changed this player
	m_bNeedToRestore = true;  // we changed at least one player
	restore->m_fFlags = flags; // we need to restore these flags
	change->m_fFlags = flags; // we have changed these flags

	if (sv_showlagcompensation.GetInt() == 1)
	{
		pPlayer->DrawServerHitboxes(4, true);
	}
}


void CLagCompensationManager::FinishLagCompensation(CBasePlayer *player)
{
	VPROF_BUDGET_FLAGS("FinishLagCompensation", VPROF_BUDGETGROUP_OTHER_NETWORKING, BUDGETFLAG_CLIENT | BUDGETFLAG_SERVER);

	m_pCurrentPlayer = NULL;

	if (!m_bNeedToRestore)
	{
		m_isCurrentlyDoingCompensation = false;
		return; // no player was changed at all
	}

	// Iterate all active players
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		int pl_index = i - 1;

		if (!m_RestorePlayer.Get(pl_index))
		{
			// player wasn't changed by lag compensation
			continue;
		}

		CBasePlayer *pPlayer = UTIL_PlayerByIndex(i);
		if (!pPlayer)
		{
			continue;
		}

		LagRecord *restore = &m_RestoreData[pl_index];
		LagRecord *change = &m_ChangeData[pl_index];

		bool restoreSimulationTime = false;

		if (restore->m_fFlags & LC_SIZE_CHANGED)
		{
			restoreSimulationTime = true;

			// see if simulation made any changes, if no, then do the restore, otherwise,
			//  leave new values in
			if (pPlayer->CollisionProp()->OBBMinsPreScaled() == change->m_vecMinsPreScaled &&
				pPlayer->CollisionProp()->OBBMaxsPreScaled() == change->m_vecMaxsPreScaled)
			{
				// Restore it
				pPlayer->SetSize(restore->m_vecMinsPreScaled, restore->m_vecMaxsPreScaled);
			}
#ifdef STAGING_ONLY
			else
			{
				Warning("Should we really not restore the size?\n");
			}
#endif
		}

		if (restore->m_fFlags & LC_ANGLES_CHANGED)
		{
			restoreSimulationTime = true;

			if (pPlayer->GetLocalAngles() == change->m_vecAngles)
			{
				pPlayer->SetLocalAngles(restore->m_vecAngles);
			}
		}

		if (restore->m_fFlags & LC_ORIGIN_CHANGED)
		{
			restoreSimulationTime = true;

			// Okay, let's see if we can do something reasonable with the change
			Vector delta = pPlayer->GetLocalOrigin() - change->m_vecOrigin;

			// If it moved really far, just leave the player in the new spot!!!
			if (delta.Length2DSqr() < m_flTeleportDistanceSqr)
			{
				RestorePlayerTo(pPlayer, restore->m_vecOrigin + delta);
			}
		}

		if (restore->m_fFlags & LC_ANIMATION_CHANGED)
		{
			restoreSimulationTime = true;

			pPlayer->SetSequence(restore->m_masterSequence);
			pPlayer->SetCycle(restore->m_masterCycle);

			int layerCount = pPlayer->GetNumAnimOverlays();
			for (int layerIndex = 0; layerIndex < layerCount; ++layerIndex)
			{
				CAnimationLayer *currentLayer = pPlayer->GetAnimOverlay(layerIndex);
				if (currentLayer)
				{
					currentLayer->m_flCycle = restore->m_layerRecords[layerIndex].m_cycle;
					currentLayer->m_nOrder = restore->m_layerRecords[layerIndex].m_order;
					currentLayer->m_nSequence = restore->m_layerRecords[layerIndex].m_sequence;
					currentLayer->m_flWeight = restore->m_layerRecords[layerIndex].m_weight;
				}
			}
		}

		if (restoreSimulationTime)
		{
			pPlayer->SetSimulationTime(restore->m_flSimulationTime);
		}
	}

	m_isCurrentlyDoingCompensation = false;
}
#endif