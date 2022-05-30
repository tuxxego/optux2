//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef WEAPON_INFECTIONHANDS_H
#define WEAPON_INFECTIONHANDS_H

#include "basebludgeonweapon.h"

#if defined( _WIN32 )
#pragma once
#endif

#ifdef HL2MP
#error weapon_infectionhands.h must not be included in hl2mp. The windows compiler will use the wrong class elsewhere if it is.
#endif

#define	INFECTIONHANDS_RANGE	75.0f
#define	INFECTIONHANDS_REFIRE	0.4f

//-----------------------------------------------------------------------------
// CWeaponInfectionHands
//-----------------------------------------------------------------------------

class CWeaponInfectionHands : public CBaseHLBludgeonWeapon
{
public:
	DECLARE_CLASS( CWeaponInfectionHands, CBaseHLBludgeonWeapon );

	DECLARE_SERVERCLASS();
	DECLARE_ACTTABLE();

	CWeaponInfectionHands();

	float		GetRange( void )		{	return	INFECTIONHANDS_RANGE;	}
	float		GetFireRate( void )		{	return	INFECTIONHANDS_REFIRE;	}

	void		AddViewKick( void );
	float		GetDamageForActivity( Activity hitActivity );

	virtual int WeaponMeleeAttack1Condition( float flDot, float flDist );
	void		SecondaryAttack( void )	{	return;	}

	// Animation event
	virtual void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

private:
	// Animation event handlers
	void HandleAnimEventMeleeHit( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
};

#endif // WEAPON_INFECTIONHANDS_H
