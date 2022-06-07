//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A slow-moving, once-human headcrab victim with only melee attacks.
//
//=============================================================================//

#include "cbase.h"

#include "doors.h"

#include "simtimer.h"
#include "npc_BaseDeadEye.h"
#include "ai_hull.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "gib.h"
#include "soundenvelope.h"
#include "engine/IEngineSound.h"
#include "ammodef.h"
#ifdef MAPBASE
#include "AI_ResponseSystem.h"
#include "ai_speech.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// ACT_FLINCH_PHYSICS


ConVar	sk_deadeye_health("sk_deadeye_health", "0");

envelopePoint_t envDeadEyeMoanVolumeFast[] =
{
	{ 7.0f, 7.0f,
	0.1f, 0.1f,
	},
	{ 0.0f, 0.0f,
	0.2f, 0.3f,
	},
};

envelopePoint_t envDeadEyeMoanVolume[] =
{
	{ 1.0f, 1.0f,
	0.1f, 0.1f,
	},
	{ 1.0f, 1.0f,
	0.2f, 0.2f,
	},
	{ 0.0f, 0.0f,
	0.3f, 0.4f,
	},
};

envelopePoint_t envDeadEyeMoanVolumeLong[] =
{
	{ 1.0f, 1.0f,
	0.3f, 0.5f,
	},
	{ 1.0f, 1.0f,
	0.6f, 1.0f,
	},
	{ 0.0f, 0.0f,
	0.3f, 0.4f,
	},
};

envelopePoint_t envDeadEyeMoanIgnited[] =
{
	{ 1.0f, 1.0f,
	0.5f, 1.0f,
	},
	{ 1.0f, 1.0f,
	30.0f, 30.0f,
	},
	{ 0.0f, 0.0f,
	0.5f, 1.0f,
	},
};

#ifdef MAPBASE
//------------------------------------------------------------------------------
// Move these to CNPC_BaseDeadEye if other deadeyes end up using the response system
//------------------------------------------------------------------------------
#define TLK_DEADEYE_PAIN "TLK_WOUND"
#define TLK_DEADEYE_DEATH "TLK_DEATH"
#define TLK_DEADEYE_ALERT "TLK_STARTCOMBAT"
#define TLK_DEADEYE_IDLE "TLK_QUESTION"
#define TLK_DEADEYE_ATTACK "TLK_MELEE"
#define TLK_DEADEYE_MOAN "TLK_MOAN"
#endif


//=============================================================================
//=============================================================================

class CDeadEye : public CAI_BlendingHost<CNPC_BaseDeadEye>
{
	DECLARE_DATADESC();
	DECLARE_CLASS(CDeadEye, CAI_BlendingHost<CNPC_BaseDeadEye>);

public:
	CDeadEye()
		: m_DurationDoorBash(2, 6),
		m_NextTimeToStartDoorBash(3.0)
	{
	}

	void Spawn(void);
	void Precache(void);

	void SetDeadEyeModel(void);
	void MoanSound(envelopePoint_t *pEnvelope, int iEnvelopeSize);
	bool ShouldBecomeTorso(const CTakeDamageInfo &info, float flDamageThreshold);
	bool CanBecomeLiveTorso() { return !m_fIsHeadless; }

	void GatherConditions(void);

	int SelectFailSchedule(int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode);
	int TranslateSchedule(int scheduleType);

#ifndef HL2_EPISODIC
	void CheckFlinches() {} // DeadEye has custom flinch code
#endif // HL2_EPISODIC

	Activity NPC_TranslateActivity(Activity newActivity);

	void OnStateChange(NPC_STATE OldState, NPC_STATE NewState);

	void StartTask(const Task_t *pTask);
	void RunTask(const Task_t *pTask);

	virtual const char *GetLegsModel(void);
	virtual const char *GetTorsoModel(void);
	virtual const char *GetHeadcrabClassname(void);
	virtual const char *GetHeadcrabModel(void);

	virtual bool OnObstructingDoor(AILocalMoveGoal_t *pMoveGoal,
		CBaseDoor *pDoor,
		float distClear,
		AIMoveResult_t *pResult);

	Activity SelectDoorBash();

	void Ignite(float flFlameLifetime, bool bNPCOnly = true, float flSize = 0.0f, bool bCalledByLevelDesigner = false);
	void Extinguish();
	int OnTakeDamage_Alive(const CTakeDamageInfo &inputInfo);
	bool IsHeavyDamage(const CTakeDamageInfo &info);
	bool IsSquashed(const CTakeDamageInfo &info);
	void BuildScheduleTestBits(void);

	void PrescheduleThink(void);
	int SelectSchedule(void);

	void PainSound(const CTakeDamageInfo &info);
	void DeathSound(const CTakeDamageInfo &info);
	void AlertSound(void);
	void IdleSound(void);
	void AttackSound(void);
	void AttackHitSound(void);
	void AttackMissSound(void);
	void FootstepSound(bool fRightFoot);
	void FootscuffSound(bool fRightFoot);

	const char *GetMoanSound(int nSound);

public:
	DEFINE_CUSTOM_AI;

protected:
	static const char *pMoanSounds[];


private:
	CHandle< CBaseDoor > m_hBlockingDoor;
	float				 m_flDoorBashYaw;

	CRandSimTimer 		 m_DurationDoorBash;
	CSimTimer 	  		 m_NextTimeToStartDoorBash;

	Vector				 m_vPositionCharged;
};

LINK_ENTITY_TO_CLASS(npc_deadeye, CDeadEye);
LINK_ENTITY_TO_CLASS(npc_deadeye_torso, CDeadEye);

//---------------------------------------------------------
//---------------------------------------------------------
const char *CDeadEye::pMoanSounds[] =
{
	"NPC_BaseDeadEye.Moan1",
	"NPC_BaseDeadEye.Moan2",
	"NPC_BaseDeadEye.Moan3",
	"NPC_BaseDeadEye.Moan4",
};

//=========================================================
// Conditions
//=========================================================
enum
{
	COND_BLOCKED_BY_DOOR = LAST_BASE_DEADEYE_CONDITION,
	COND_DOOR_OPENED,
	COND_DEADEYE_CHARGE_TARGET_MOVED,
};

//=========================================================
// Schedules
//=========================================================
enum
{
	SCHED_DEADEYE_BASH_DOOR = LAST_BASE_DEADEYE_SCHEDULE,
	SCHED_DEADEYE_WANDER_ANGRILY,
	SCHED_DEADEYE_CHARGE_ENEMY,
	SCHED_DEADEYE_FAIL,
};

//=========================================================
// Tasks
//=========================================================
enum
{
	TASK_DEADEYE_EXPRESS_ANGER = LAST_BASE_DEADEYE_TASK,
	TASK_DEADEYE_YAW_TO_DOOR,
	TASK_DEADEYE_ATTACK_DOOR,
	TASK_DEADEYE_CHARGE_ENEMY,
};

//-----------------------------------------------------------------------------

int ACT_DEADEYE_TANTRUM;
int ACT_DEADEYE_WALLPOUND;

BEGIN_DATADESC(CDeadEye)

DEFINE_FIELD(m_hBlockingDoor, FIELD_EHANDLE),
DEFINE_FIELD(m_flDoorBashYaw, FIELD_FLOAT),
DEFINE_EMBEDDED(m_DurationDoorBash),
DEFINE_EMBEDDED(m_NextTimeToStartDoorBash),
DEFINE_FIELD(m_vPositionCharged, FIELD_POSITION_VECTOR),

END_DATADESC()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDeadEye::Precache(void)
{
	BaseClass::Precache();

	PrecacheModel("models/zombie/classic.mdl");
	PrecacheModel("models/zombie/classic_torso.mdl");
	PrecacheModel("models/zombie/classic_legs.mdl");

	PrecacheScriptSound("DeadEye.FootstepRight");
	PrecacheScriptSound("DeadEye.FootstepLeft");
	PrecacheScriptSound("DeadEye.FootstepLeft");
	PrecacheScriptSound("DeadEye.ScuffRight");
	PrecacheScriptSound("DeadEye.ScuffLeft");
	PrecacheScriptSound("DeadEye.AttackHit");
	PrecacheScriptSound("DeadEye.AttackMiss");
	PrecacheScriptSound("DeadEye.Pain");
	PrecacheScriptSound("DeadEye.Die");
	PrecacheScriptSound("DeadEye.Alert");
	PrecacheScriptSound("DeadEye.Idle");
	PrecacheScriptSound("DeadEye.Attack");

	PrecacheScriptSound("NPC_BaseDeadEye.Moan1");
	PrecacheScriptSound("NPC_BaseDeadEye.Moan2");
	PrecacheScriptSound("NPC_BaseDeadEye.Moan3");
	PrecacheScriptSound("NPC_BaseDeadEye.Moan4");
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDeadEye::Spawn(void)
{
	Precache();

#ifdef MAPBASE
	if (Q_strstr(GetClassname(), "torso"))
	{
		// This was placed as an npc_deadeye_torso
		m_fIsTorso = true;
	}
	else
	{
		m_fIsTorso = false;
	}
#else
	if (FClassnameIs(this, "npc_deadeye"))
	{
		m_fIsTorso = false;
	}
	else
	{
		// This was placed as an npc_deadeye_torso
		m_fIsTorso = true;
	}

	m_fIsHeadless = true;
#endif

#ifdef HL2_EPISODIC
	SetBloodColor(BLOOD_COLOR_ZOMBIE);
#else
	SetBloodColor(BLOOD_COLOR_GREEN);
#endif // HL2_EPISODIC

	m_iHealth = sk_deadeye_health.GetFloat();
	m_flFieldOfView = 0.2;

	CapabilitiesClear();

	//GetNavigator()->SetRememberStaleNodes( false );

	BaseClass::Spawn();

	m_flNextMoanSound = gpGlobals->curtime + random->RandomFloat(1.0, 4.0);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDeadEye::PrescheduleThink(void)
{
	if (gpGlobals->curtime > m_flNextMoanSound)
	{
		if (CanPlayMoanSound())
		{
			// Classic guy idles instead of moans.
			IdleSound();

			m_flNextMoanSound = gpGlobals->curtime + random->RandomFloat(2.0, 5.0);
		}
		else
		{
			m_flNextMoanSound = gpGlobals->curtime + random->RandomFloat(1.0, 2.0);
		}
	}

	BaseClass::PrescheduleThink();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CDeadEye::SelectSchedule(void)
{
	if (HasCondition(COND_PHYSICS_DAMAGE) && !m_ActBusyBehavior.IsActive())
	{
		return SCHED_FLINCH_PHYSICS;
	}

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: Sound of a footstep
//-----------------------------------------------------------------------------
void CDeadEye::FootstepSound(bool fRightFoot)
{
	if (fRightFoot)
	{
		EmitSound("DeadEye.FootstepRight");
	}
	else
	{
		EmitSound("DeadEye.FootstepLeft");
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sound of a foot sliding/scraping
//-----------------------------------------------------------------------------
void CDeadEye::FootscuffSound(bool fRightFoot)
{
	if (fRightFoot)
	{
		EmitSound("DeadEye.ScuffRight");
	}
	else
	{
		EmitSound("DeadEye.ScuffLeft");
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack hit sound
//-----------------------------------------------------------------------------
void CDeadEye::AttackHitSound(void)
{
	EmitSound("DeadEye.AttackHit");
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack miss sound
//-----------------------------------------------------------------------------
void CDeadEye::AttackMissSound(void)
{
	// Play a random attack miss sound
	EmitSound("DeadEye.AttackMiss");
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDeadEye::PainSound(const CTakeDamageInfo &info)
{
	// We're constantly taking damage when we are on fire. Don't make all those noises!
	if (IsOnFire())
	{
		return;
	}

	EmitSound("DeadEye.Pain");
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDeadEye::DeathSound(const CTakeDamageInfo &info)
{
	EmitSound("DeadEye.Die");
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDeadEye::AlertSound(void)
{
	EmitSound("DeadEye.Alert");

	// Don't let a moan sound cut off the alert sound.
	m_flNextMoanSound += random->RandomFloat(2.0, 4.0);
}

//-----------------------------------------------------------------------------
// Purpose: Returns a moan sound for this class of deadeye.
//-----------------------------------------------------------------------------
const char *CDeadEye::GetMoanSound(int nSound)
{
	return pMoanSounds[nSound % ARRAYSIZE(pMoanSounds)];
}

//-----------------------------------------------------------------------------
// Purpose: Play a random idle sound.
//-----------------------------------------------------------------------------
void CDeadEye::IdleSound(void)
{
	if (GetState() == NPC_STATE_IDLE && random->RandomFloat(0, 1) == 0)
	{
		// Moan infrequently in IDLE state.
		return;
	}

	if (IsSlumped())
	{
		// Sleeping deadeyes are quiet.
		return;
	}

	EmitSound("DeadEye.Idle");
	MakeAISpookySound(360.0f);
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack sound.
//-----------------------------------------------------------------------------
void CDeadEye::AttackSound(void)
{
	EmitSound("DeadEye.Attack");
}

//-----------------------------------------------------------------------------
// Purpose: Returns the classname (ie "npc_headcrab") to spawn when our headcrab bails.
//-----------------------------------------------------------------------------
const char *CDeadEye::GetHeadcrabClassname(void)
{
	return "npc_headcrab";
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CDeadEye::GetHeadcrabModel(void)
{
	return "models/headcrabclassic.mdl";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CDeadEye::GetLegsModel(void)
{
	return "models/zombie/classic_legs.mdl";
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CDeadEye::GetTorsoModel(void)
{
	return "models/zombie/classic_torso.mdl";
}


//---------------------------------------------------------
//---------------------------------------------------------
void CDeadEye::SetDeadEyeModel(void)
{
	Hull_t lastHull = GetHullType();

	if (m_fIsTorso)
	{
		SetModel("models/zombie/classic_torso.mdl");
		SetHullType(HULL_TINY);
	}
	else
	{
		SetModel("models/zombie/classic.mdl");
		SetHullType(HULL_HUMAN);
	}

	SetBodygroup(DEADEYE_BODYGROUP_HEADCRAB, !m_fIsHeadless);

	SetHullSizeNormal(true);
	SetDefaultEyeOffset();
	SetActivity(ACT_IDLE);

	// hull changed size, notify vphysics
	// UNDONE: Solve this generally, systematically so other
	// NPCs can change size
	if (lastHull != GetHullType())
	{
		if (VPhysicsGetObject())
		{
			SetupVPhysicsHull();
		}
	}
}

//---------------------------------------------------------
// Classic deadeye only uses moan sound if on fire.
//---------------------------------------------------------
void CDeadEye::MoanSound(envelopePoint_t *pEnvelope, int iEnvelopeSize)
{
	if (IsOnFire())
	{
		BaseClass::MoanSound(pEnvelope, iEnvelopeSize);
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CDeadEye::ShouldBecomeTorso(const CTakeDamageInfo &info, float flDamageThreshold)
{
	if (IsSlumped())
	{
		// Never break apart a slouched deadeye. This is because the most fun
		// slouched deadeyes to kill are ones sleeping leaning against explosive
		// barrels. If you break them in half in the blast, the force of being
		// so close to the explosion makes the body pieces fly at ridiculous 
		// velocities because the pieces weigh less than the whole.
		return false;
	}

	return BaseClass::ShouldBecomeTorso(info, flDamageThreshold);
}

//---------------------------------------------------------
//---------------------------------------------------------
void CDeadEye::GatherConditions(void)
{
	BaseClass::GatherConditions();

	static int conditionsToClear[] =
	{
		COND_BLOCKED_BY_DOOR,
		COND_DOOR_OPENED,
		COND_DEADEYE_CHARGE_TARGET_MOVED,
	};

	ClearConditions(conditionsToClear, ARRAYSIZE(conditionsToClear));

	if (m_hBlockingDoor == NULL ||
		(m_hBlockingDoor->m_toggle_state == TS_AT_TOP ||
		m_hBlockingDoor->m_toggle_state == TS_GOING_UP))
	{
		ClearCondition(COND_BLOCKED_BY_DOOR);
		if (m_hBlockingDoor != NULL)
		{
			SetCondition(COND_DOOR_OPENED);
			m_hBlockingDoor = NULL;
		}
	}
	else
		SetCondition(COND_BLOCKED_BY_DOOR);

	if (ConditionInterruptsCurSchedule(COND_DEADEYE_CHARGE_TARGET_MOVED))
	{
		if (GetNavigator()->IsGoalActive())
		{
			const float CHARGE_RESET_TOLERANCE = 60.0;
			if (!GetEnemy() ||
				(m_vPositionCharged - GetEnemyLKP()).Length() > CHARGE_RESET_TOLERANCE)
			{
				SetCondition(COND_DEADEYE_CHARGE_TARGET_MOVED);
			}

		}
	}
}

//---------------------------------------------------------
//---------------------------------------------------------

int CDeadEye::SelectFailSchedule(int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode)
{
	if (HasCondition(COND_BLOCKED_BY_DOOR) && m_hBlockingDoor != NULL)
	{
		ClearCondition(COND_BLOCKED_BY_DOOR);
		if (m_NextTimeToStartDoorBash.Expired() && failedSchedule != SCHED_DEADEYE_BASH_DOOR)
			return SCHED_DEADEYE_BASH_DOOR;
		m_hBlockingDoor = NULL;
	}

	if (failedSchedule != SCHED_DEADEYE_CHARGE_ENEMY &&
		IsPathTaskFailure(taskFailCode) &&
		random->RandomInt(1, 100) < 50)
	{
		return SCHED_DEADEYE_CHARGE_ENEMY;
	}

	if (failedSchedule != SCHED_DEADEYE_WANDER_ANGRILY &&
		(failedSchedule == SCHED_TAKE_COVER_FROM_ENEMY ||
		failedSchedule == SCHED_CHASE_ENEMY_FAILED))
	{
		return SCHED_DEADEYE_WANDER_ANGRILY;
	}

	return BaseClass::SelectFailSchedule(failedSchedule, failedTask, taskFailCode);
}

//---------------------------------------------------------
//---------------------------------------------------------

int CDeadEye::TranslateSchedule(int scheduleType)
{
	if (scheduleType == SCHED_COMBAT_FACE && IsUnreachable(GetEnemy()))
		return SCHED_TAKE_COVER_FROM_ENEMY;

	if (!m_fIsTorso && scheduleType == SCHED_FAIL)
		return SCHED_DEADEYE_FAIL;

	return BaseClass::TranslateSchedule(scheduleType);
}

//---------------------------------------------------------

Activity CDeadEye::NPC_TranslateActivity(Activity newActivity)
{
	newActivity = BaseClass::NPC_TranslateActivity(newActivity);

	if (newActivity == ACT_RUN)
		return ACT_SPRINT;

	if (m_fIsTorso && (newActivity == ACT_DEADEYE_TANTRUM))
		return ACT_IDLE;

	return newActivity;
}

//---------------------------------------------------------
//---------------------------------------------------------
void CDeadEye::OnStateChange(NPC_STATE OldState, NPC_STATE NewState)
{
	BaseClass::OnStateChange(OldState, NewState);
}

//---------------------------------------------------------
//---------------------------------------------------------

void CDeadEye::StartTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_DEADEYE_EXPRESS_ANGER:
	{
		if (random->RandomInt(1, 4) == 2)
		{
			SetIdealActivity((Activity)ACT_DEADEYE_TANTRUM);
		}
		else
		{
			TaskComplete();
		}

		break;
	}

	case TASK_DEADEYE_YAW_TO_DOOR:
	{
		AssertMsg(m_hBlockingDoor != NULL, "Expected condition handling to break schedule before landing here");
		if (m_hBlockingDoor != NULL)
		{
			GetMotor()->SetIdealYaw(m_flDoorBashYaw);
		}
		TaskComplete();
		break;
	}

	case TASK_DEADEYE_ATTACK_DOOR:
	{
		m_DurationDoorBash.Reset();
		SetIdealActivity(SelectDoorBash());
		break;
	}

	case TASK_DEADEYE_CHARGE_ENEMY:
	{
		if (!GetEnemy())
			TaskFail(FAIL_NO_ENEMY);
		else if (GetNavigator()->SetVectorGoalFromTarget(GetEnemy()->GetLocalOrigin()))
		{
			m_vPositionCharged = GetEnemy()->GetLocalOrigin();
			TaskComplete();
		}
		else
			TaskFail(FAIL_NO_ROUTE);
		break;
	}

	default:
		BaseClass::StartTask(pTask);
		break;
	}
}

//---------------------------------------------------------
//---------------------------------------------------------

void CDeadEye::RunTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_DEADEYE_ATTACK_DOOR:
	{
		if (IsActivityFinished())
		{
			if (m_DurationDoorBash.Expired())
			{
				TaskComplete();
				m_NextTimeToStartDoorBash.Reset();
			}
			else
				ResetIdealActivity(SelectDoorBash());
		}
		break;
	}

	case TASK_DEADEYE_CHARGE_ENEMY:
	{
		break;
	}

	case TASK_DEADEYE_EXPRESS_ANGER:
	{
		if (IsActivityFinished())
		{
			TaskComplete();
		}
		break;
	}

	default:
		BaseClass::RunTask(pTask);
		break;
	}
}

//---------------------------------------------------------
//---------------------------------------------------------

bool CDeadEye::OnObstructingDoor(AILocalMoveGoal_t *pMoveGoal, CBaseDoor *pDoor,
	float distClear, AIMoveResult_t *pResult)
{
	if (BaseClass::OnObstructingDoor(pMoveGoal, pDoor, distClear, pResult))
	{
		if (IsMoveBlocked(*pResult) && pMoveGoal->directTrace.vHitNormal != vec3_origin)
		{
			m_hBlockingDoor = pDoor;
			m_flDoorBashYaw = UTIL_VecToYaw(pMoveGoal->directTrace.vHitNormal * -1);
		}
		return true;
	}

	return false;
}

//---------------------------------------------------------
//---------------------------------------------------------

Activity CDeadEye::SelectDoorBash()
{
	if (random->RandomInt(1, 3) == 1)
		return ACT_MELEE_ATTACK1;
	return (Activity)ACT_DEADEYE_WALLPOUND;
}

//---------------------------------------------------------
// DeadEyes should scream continuously while burning, so long
// as they are alive... but NOT IN GERMANY!
//---------------------------------------------------------
void CDeadEye::Ignite(float flFlameLifetime, bool bNPCOnly, float flSize, bool bCalledByLevelDesigner)
{
	if (!IsOnFire() && IsAlive())
	{
		BaseClass::Ignite(flFlameLifetime, bNPCOnly, flSize, bCalledByLevelDesigner);

		if (!UTIL_IsLowViolence())
		{
			RemoveSpawnFlags(SF_NPC_GAG);

			MoanSound(envDeadEyeMoanIgnited, ARRAYSIZE(envDeadEyeMoanIgnited));

			if (m_pMoanSound)
			{
				ENVELOPE_CONTROLLER.SoundChangePitch(m_pMoanSound, 120, 1.0);
				ENVELOPE_CONTROLLER.SoundChangeVolume(m_pMoanSound, 1, 1.0);
			}
		}
	}
}

//---------------------------------------------------------
// If a deadeye stops burning and hasn't died, quiet him down
//---------------------------------------------------------
void CDeadEye::Extinguish()
{
	if (m_pMoanSound)
	{
		ENVELOPE_CONTROLLER.SoundChangeVolume(m_pMoanSound, 0, 2.0);
		ENVELOPE_CONTROLLER.SoundChangePitch(m_pMoanSound, 100, 2.0);
		m_flNextMoanSound = gpGlobals->curtime + random->RandomFloat(2.0, 4.0);
	}

	BaseClass::Extinguish();
}

//---------------------------------------------------------
//---------------------------------------------------------
int CDeadEye::OnTakeDamage_Alive(const CTakeDamageInfo &inputInfo)
{
#ifndef HL2_EPISODIC
	if (inputInfo.GetDamageType() & DMG_BUCKSHOT)
	{
		if (!m_fIsTorso && inputInfo.GetDamage() > (m_iMaxHealth / 3))
		{
			// Always flinch if damaged a lot by buckshot, even if not shot in the head.
			// The reason for making sure we did at least 1/3rd of the deadeye's max health
			// is so the deadeye doesn't flinch every time the odd shotgun pellet hits them,
			// and so the maximum number of times you'll see a deadeye flinch like this is 2.(sjb)
			AddGesture(ACT_GESTURE_FLINCH_HEAD);
		}
	}
#endif // HL2_EPISODIC

	return BaseClass::OnTakeDamage_Alive(inputInfo);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CDeadEye::IsHeavyDamage(const CTakeDamageInfo &info)
{
#ifdef HL2_EPISODIC
	if (info.GetDamageType() & DMG_BUCKSHOT)
	{
		if (!m_fIsTorso && info.GetDamage() > (m_iMaxHealth / 3))
			return true;
	}

	// Randomly treat all damage as heavy
	if (info.GetDamageType() & (DMG_BULLET | DMG_BUCKSHOT))
	{
		// Don't randomly flinch if I'm melee attacking
		if (!HasCondition(COND_CAN_MELEE_ATTACK1) && (RandomFloat() > 0.5))
		{
			// Randomly forget I've flinched, so that I'll be forced to play a big flinch
			// If this doesn't happen, it means I may not fully flinch if I recently flinched
			if (RandomFloat() > 0.75)
			{
				Forget(bits_MEMORY_FLINCHED);
			}

			return true;
		}
	}
#endif // HL2_EPISODIC

	return BaseClass::IsHeavyDamage(info);
}

//---------------------------------------------------------
//---------------------------------------------------------
#define DEADEYE_SQUASH_MASS	300.0f  // Anything this heavy or heavier squashes a deadeye good. (show special fx)
bool CDeadEye::IsSquashed(const CTakeDamageInfo &info)
{
	if (GetHealth() > 0)
	{
		return false;
	}

	if (info.GetDamageType() & DMG_CRUSH && info.GetInflictor()) // Mapbase - Fixes a crash with inflictor-less crush damage
	{
		IPhysicsObject *pCrusher = info.GetInflictor()->VPhysicsGetObject();
		if (pCrusher && pCrusher->GetMass() >= DEADEYE_SQUASH_MASS && info.GetInflictor()->WorldSpaceCenter().z > EyePosition().z)
		{
			// This heuristic detects when a deadeye has been squashed from above by a heavy
			// item. Done specifically so we can add gore effects to Ravenholm cartraps.
			// The deadeye must take physics damage from a 300+kg object that is centered above its eyes (comes from above)
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------
//---------------------------------------------------------
void CDeadEye::BuildScheduleTestBits(void)
{
	BaseClass::BuildScheduleTestBits();

	if (!m_fIsTorso && !IsCurSchedule(SCHED_FLINCH_PHYSICS) && !m_ActBusyBehavior.IsActive())
	{
		SetCustomInterruptCondition(COND_PHYSICS_DAMAGE);
	}
}


//=============================================================================

AI_BEGIN_CUSTOM_NPC(npc_deadeye, CDeadEye)

DECLARE_CONDITION(COND_BLOCKED_BY_DOOR)
DECLARE_CONDITION(COND_DOOR_OPENED)
DECLARE_CONDITION(COND_DEADEYE_CHARGE_TARGET_MOVED)

DECLARE_TASK(TASK_DEADEYE_EXPRESS_ANGER)
DECLARE_TASK(TASK_DEADEYE_YAW_TO_DOOR)
DECLARE_TASK(TASK_DEADEYE_ATTACK_DOOR)
DECLARE_TASK(TASK_DEADEYE_CHARGE_ENEMY)

DECLARE_ACTIVITY(ACT_DEADEYE_TANTRUM);
DECLARE_ACTIVITY(ACT_DEADEYE_WALLPOUND);

DEFINE_SCHEDULE
(
SCHED_DEADEYE_BASH_DOOR,

"	Tasks"
"		TASK_SET_ACTIVITY				ACTIVITY:ACT_DEADEYE_TANTRUM"
"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_TAKE_COVER_FROM_ENEMY"
"		TASK_DEADEYE_YAW_TO_DOOR			0"
"		TASK_FACE_IDEAL					0"
"		TASK_DEADEYE_ATTACK_DOOR			0"
""
"	Interrupts"
"		COND_DEADEYE_RELEASECRAB"
"		COND_ENEMY_DEAD"
"		COND_NEW_ENEMY"
"		COND_DOOR_OPENED"
)

DEFINE_SCHEDULE
(
SCHED_DEADEYE_WANDER_ANGRILY,

"	Tasks"
"		TASK_WANDER						480240" // 48 units to 240 units.
"		TASK_WALK_PATH					0"
"		TASK_WAIT_FOR_MOVEMENT			4"
""
"	Interrupts"
"		COND_DEADEYE_RELEASECRAB"
"		COND_ENEMY_DEAD"
"		COND_NEW_ENEMY"
"		COND_DOOR_OPENED"
)

DEFINE_SCHEDULE
(
SCHED_DEADEYE_CHARGE_ENEMY,


"	Tasks"
"		TASK_DEADEYE_CHARGE_ENEMY		0"
"		TASK_WALK_PATH					0"
"		TASK_WAIT_FOR_MOVEMENT			0"
"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_DEADEYE_TANTRUM" /* placeholder until frustration/rage/fence shake animation available */
""
"	Interrupts"
"		COND_DEADEYE_RELEASECRAB"
"		COND_ENEMY_DEAD"
"		COND_NEW_ENEMY"
"		COND_DOOR_OPENED"
"		COND_DEADEYE_CHARGE_TARGET_MOVED"
)

DEFINE_SCHEDULE
(
SCHED_DEADEYE_FAIL,

"	Tasks"
"		TASK_STOP_MOVING		0"
"		TASK_SET_ACTIVITY		ACTIVITY:ACT_DEADEYE_TANTRUM"
"		TASK_WAIT				1"
"		TASK_WAIT_PVS			0"
""
"	Interrupts"
"		COND_CAN_RANGE_ATTACK1 "
"		COND_CAN_RANGE_ATTACK2 "
"		COND_CAN_MELEE_ATTACK1 "
"		COND_CAN_MELEE_ATTACK2"
"		COND_GIVE_WAY"
"		COND_DOOR_OPENED"
)

AI_END_CUSTOM_NPC()

//=============================================================================

#ifdef MAPBASE
class CDeadEyeCustom : public CAI_ExpresserHost<CDeadEye>
{
	DECLARE_DATADESC();
	DECLARE_CLASS(CDeadEyeCustom, CAI_ExpresserHost<CDeadEye>);

public:
	CDeadEyeCustom();

	void Spawn(void);
	void Precache(void);

	void SpeakIfAllowed(const char *concept, AI_CriteriaSet *modifiers = NULL);
	void ModifyOrAppendCriteria(AI_CriteriaSet& set);
	virtual CAI_Expresser *CreateExpresser(void);
	virtual CAI_Expresser *GetExpresser() { return m_pExpresser; }
	virtual void		PostConstructor(const char *szClassname);

	void PainSound(const CTakeDamageInfo &info);
	void DeathSound(const CTakeDamageInfo &info);
	void AlertSound(void);
	void IdleSound(void);
	void AttackSound(void);

	const char *GetMoanSound(int nSound);

	void SetDeadEyeModel(void);

	virtual const char *GetLegsModel(void) { return STRING(m_iszLegsModel); }
	virtual const char *GetTorsoModel(void) { return STRING(m_iszTorsoModel); }
	virtual const char *GetHeadcrabClassname(void) { return STRING(m_iszHeadcrabClassname); }
	virtual const char *GetHeadcrabModel(void) { return STRING(m_iszHeadcrabModel); }

	string_t m_iszLegsModel;
	string_t m_iszTorsoModel;
	string_t m_iszHeadcrabClassname;
	string_t m_iszHeadcrabModel;

	CAI_Expresser *m_pExpresser;
};

BEGIN_DATADESC(CDeadEyeCustom)

DEFINE_KEYFIELD(m_iszLegsModel, FIELD_STRING, "LegsModel"),
DEFINE_KEYFIELD(m_iszTorsoModel, FIELD_STRING, "TorsoModel"),
DEFINE_KEYFIELD(m_iszHeadcrabClassname, FIELD_STRING, "HeadcrabClassname"),
DEFINE_KEYFIELD(m_iszHeadcrabModel, FIELD_STRING, "HeadcrabModel"),

END_DATADESC()

LINK_ENTITY_TO_CLASS(npc_deadeye_custom, CDeadEyeCustom);
LINK_ENTITY_TO_CLASS(npc_deadeye_custom_torso, CDeadEyeCustom);


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDeadEyeCustom::CDeadEyeCustom()
{
	m_iszLegsModel = AllocPooledString(CDeadEye::GetLegsModel());
	m_iszTorsoModel = AllocPooledString(CDeadEye::GetTorsoModel());
	m_iszHeadcrabClassname = AllocPooledString(CDeadEye::GetHeadcrabClassname());
	m_iszHeadcrabModel = AllocPooledString(CDeadEye::GetHeadcrabModel());

	SetModelName(AllocPooledString("models/zombie/classic.mdl"));
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDeadEyeCustom::Spawn(void)
{
	int iHealth = m_iHealth;

	BaseClass::Spawn();

	if (iHealth > 0)
	{
		m_iMaxHealth = iHealth;
		m_iHealth = iHealth;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDeadEyeCustom::Precache(void)
{
	BaseClass::Precache();

	PrecacheModel(STRING(GetModelName()));

	if (m_iszLegsModel != NULL_STRING)
		PrecacheModel(STRING(m_iszLegsModel));

	if (m_iszTorsoModel != NULL_STRING)
		PrecacheModel(STRING(m_iszTorsoModel));

	if (m_iszHeadcrabClassname != NULL_STRING)
		UTIL_PrecacheOther(STRING(m_iszHeadcrabClassname));

	if (m_iszHeadcrabModel != NULL_STRING)
		PrecacheModel(STRING(m_iszHeadcrabModel));
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDeadEyeCustom::SetDeadEyeModel(void)
{
	Hull_t lastHull = GetHullType();

	if (m_fIsTorso)
	{
		SetModel(GetTorsoModel());
		SetHullType(HULL_TINY);
	}
	else
	{
		SetModel(STRING(GetModelName()));
		SetHullType(HULL_HUMAN);
	}

	SetBodygroup(DEADEYE_BODYGROUP_HEADCRAB, !m_fIsHeadless);

	SetHullSizeNormal(true);
	SetDefaultEyeOffset();
	SetActivity(ACT_IDLE);

	// hull changed size, notify vphysics
	// UNDONE: Solve this generally, systematically so other
	// NPCs can change size
	if (lastHull != GetHullType())
	{
		if (VPhysicsGetObject())
		{
			SetupVPhysicsHull();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDeadEyeCustom::PainSound(const CTakeDamageInfo &info)
{
	AI_CriteriaSet modifiers;
	ModifyOrAppendDamageCriteria(modifiers, info);
	SpeakIfAllowed(TLK_DEADEYE_PAIN, &modifiers);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDeadEyeCustom::DeathSound(const CTakeDamageInfo &info)
{
	AI_CriteriaSet modifiers;
	ModifyOrAppendDamageCriteria(modifiers, info);
	SpeakIfAllowed(TLK_DEADEYE_DEATH, &modifiers);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDeadEyeCustom::AlertSound(void)
{
	SpeakIfAllowed(TLK_DEADEYE_ALERT);

	// Don't let a moan sound cut off the alert sound.
	m_flNextMoanSound += random->RandomFloat(2.0, 4.0);
}

//-----------------------------------------------------------------------------
// Purpose: Returns a moan sound for this class of deadeye.
//-----------------------------------------------------------------------------
const char *CDeadEyeCustom::GetMoanSound(int nSound)
{
	AI_CriteriaSet modifiers;

	// We could probably do this through the response system alone now, but whatever.
	modifiers.AppendCriteria("moansound", UTIL_VarArgs("%i", nSound & 4));

	AI_Response *response = SpeakFindResponse(TLK_DEADEYE_MOAN, modifiers);

	if (!response)
		return "NPC_BaseDeadEye.Moan1";

	// Must be static so it could be returned
	static char szSound[128];
	response->GetName(szSound, sizeof(szSound));

	delete response;
	return szSound;
}

//-----------------------------------------------------------------------------
// Purpose: Play a random idle sound.
//-----------------------------------------------------------------------------
void CDeadEyeCustom::IdleSound(void)
{
	if (GetState() == NPC_STATE_IDLE && random->RandomFloat(0, 1) == 0)
	{
		// Moan infrequently in IDLE state.
		return;
	}

	SpeakIfAllowed(TLK_DEADEYE_IDLE);
	MakeAISpookySound(360.0f);
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack sound.
//-----------------------------------------------------------------------------
void CDeadEyeCustom::AttackSound(void)
{
	SpeakIfAllowed(TLK_DEADEYE_ATTACK);
}

//-----------------------------------------------------------------------------
// Purpose: Speak concept
//-----------------------------------------------------------------------------
void CDeadEyeCustom::SpeakIfAllowed(const char *concept, AI_CriteriaSet *modifiers)
{
	Speak(concept, modifiers ? *modifiers : AI_CriteriaSet());
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDeadEyeCustom::ModifyOrAppendCriteria(AI_CriteriaSet& set)
{
	BaseClass::ModifyOrAppendCriteria(set);

	set.AppendCriteria("slumped", IsSlumped() ? "1" : "0");

	// Does this or a name already exist?
	set.AppendCriteria("onfire", IsOnFire() ? "1" : "0");

	// Custom deadeyes (and deadeye torsos) must make deadeye sounds.
	// This can be overridden with response contexts.
	set.AppendCriteria("classname", "npc_deadeye");
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAI_Expresser *CDeadEyeCustom::CreateExpresser(void)
{
	m_pExpresser = new CAI_Expresser(this);
	if (!m_pExpresser)
		return NULL;

	m_pExpresser->Connect(this);
	return m_pExpresser;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDeadEyeCustom::PostConstructor(const char *szClassname)
{
	BaseClass::PostConstructor(szClassname);
	CreateExpresser();
}
#endif
