//========= Copyright  1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#include "basehlcombatweapon.h"

#ifndef WEAPON_GAUSS_H
#define WEAPON_GAUSS_H
#ifdef _WIN32
#pragma once
#endif

#include "te_particlesystem.h"

#define GAUSS_BEAM_SPRITE "sprites/laserbeam.vmt"

#define	GAUSS_CHARGE_TIME			0.2f
#define	MAX_GAUSS_CHARGE			16
#define	MAX_GAUSS_CHARGE_TIME		3
#define	DANGER_GAUSS_CHARGE_TIME	10

//=============================================================================
// Gauss cannon
//=============================================================================

class CWeaponTau : public CBaseHLCombatWeapon
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS(CWeaponTau, CBaseHLCombatWeapon);

	CWeaponTau(void);

	DECLARE_SERVERCLASS();

	void	Spawn(void);
	void	Precache(void);
	void	PrimaryAttack(void);
	void	SecondaryAttack(void);
	void	AddViewKick(void);

	bool	Holster(CBaseCombatWeapon *pSwitchingTo = NULL);

	void	ItemPostFrame(void);

	float	GetFireRate(void) { return 0.2f; }

	virtual const Vector &GetBulletSpread(void)
	{
		static Vector cone = VECTOR_CONE_1DEGREES;
		return cone;
	}

protected:

	void	Fire(void);
	void	ChargedFire(void);

	void	StopChargeSound(void);

	void	DrawBeam(const Vector &startPos, const Vector &endPos, float width, bool useMuzzle = false);
	void	IncreaseCharge(void);

	EHANDLE			m_hViewModel;
	float			m_flNextChargeTime;
	CSoundPatch		*m_sndCharge;

	float			m_flChargeStartTime;
	bool			m_bCharging;
	bool			m_bChargeIndicated;

	float			m_flCoilMaxVelocity;
	float			m_flCoilVelocity;
	float			m_flCoilAngle;

	DECLARE_ACTTABLE();
};

#endif // WEAPON_GAUSS_H
