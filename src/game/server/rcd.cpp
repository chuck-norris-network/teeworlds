#include <engine/shared/config.h>
#include <engine/server.h>

#include "player.h"
#include "gamecontext.h"
#include "entity.h"

#include "rcd.hpp"

#include <string>
#include <cstring>
#include <map>

std::map<std::string, int> mapName;
std::map<std::string, int> mapClan;
std::map<std::string, int> mapIP;

static char aBuf[256];

void RajhCheatDetector::ForgetAllClients()
{
	mapName.clear();
	mapClan.clear();
	mapIP.clear();
}

// unfortuately called every tick by CCharacter::Tick()
// thus check real fire with TestFire()
void RajhCheatDetector::OnFire(CPlayer * Player)
{
	// TODO: Improve regular fire detection #7
	if(CheckFastFire(Player))
		AddWarning(Player, 0);

	CCharacter *CPlayer;
	if(!(CPlayer = Player->GameServer()->GetPlayerChar(Player->GetCID())))
		return;

	vec2 Target = vec2(CPlayer->m_LatestInput.m_TargetX, CPlayer->m_LatestInput.m_TargetY);
	vec2 TargetPos = CPlayer->m_Pos + Target;

	// on every input, store and if necessary update CPlayers interpolated cl_mouse_max_distance
	float aimDistance = distance(TargetPos, CPlayer->m_Pos);
	Player->updateInterpolatedMouseMaxDist(aimDistance);
}

void RajhCheatDetector::OnHit(CPlayer * Player, int Victim)
{
	if(!Player || Player->GetCID() == Victim)
		return;

	warning_t out = 0;
	if(CheckInputPos(Player, Victim, out))
		AddWarning(Player, out);

	if(CheckReflex(Player, Victim))
		AddWarning(Player, 2);
        
        
        CheckSomethingElse(Player, Victim);
}

void RajhCheatDetector::OnTick(CPlayer * Player)
{
	CheckWarnings(Player);
}

void RajhCheatDetector::OnPlayerEnter(CPlayer * Player)
{
	if(Player == 0)
		return;

	std::map<std::string, int>::iterator itName = mapName.find(std::string(Player->Server()->ClientName(Player->GetCID())));
	bool playerFound = itName != mapName.end();

	bool clanFound = mapClan.find(std::string(Player->Server()->ClientClan(Player->GetCID()))) != mapClan.end();

	Player->Server()->GetClientAddr(Player->GetCID(), aBuf, sizeof(aBuf));
	std::string ip = std::string(aBuf);
	std::map<std::string, int>::iterator itIP = mapIP.find(ip);
	bool ipFound = itIP != mapIP.end();

	// not found
	if (!playerFound || !ipFound)
		return;

	// probably player already banned - increasing warning level
	Player->Warnings = g_Config.m_RcdMaxWarnings / 2;

	if(playerFound && ipFound && clanFound) // no doubt, this is the same guy
	{
		str_format(aBuf, sizeof(aBuf), "Welcome back: '%s' (name, ip, clan match)",Player->Server()->ClientName(Player->GetCID()));
		Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "rcd", aBuf);
	}
	else if(playerFound && clanFound) // very likely, he got a new ip
	{
		str_format(aBuf, sizeof(aBuf), "Welcome back: '%s' (name, clan match)",Player->Server()->ClientName(Player->GetCID()));
		Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "rcd", aBuf);
	}
	else if(playerFound) // well, maybe he got a new ip
	{
		str_format(aBuf, sizeof(aBuf), "Welcome back: '%s' (name match)",Player->Server()->ClientName(Player->GetCID()));
		Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "rcd", aBuf);
	}
	else if(ipFound) // time will show
	{
		str_format(aBuf, sizeof(aBuf), "Welcome back: '%s' (ip match)",Player->Server()->ClientName(Player->GetCID()));
		Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "rcd", aBuf);
	}
}

void RajhCheatDetector::OnPlayerLeave(CPlayer * Player)
{
	if(Player->Warnings >= g_Config.m_RcdMaxWarnings)
	{
		std::string name = Player->Server()->ClientName(Player->GetCID());
		std::string clan = Player->Server()->ClientClan(Player->GetCID());

		Player->Server()->GetClientAddr(Player->GetCID(), aBuf, sizeof(aBuf));
		std::string ip = std::string(aBuf);

		mapName[name] = Player->Warnings;
		mapClan[clan] = Player->Warnings;
		mapIP[ip] = Player->Warnings;
	}
}

// amount may be zero; this indicates that there was strange behaviour that is worth to update Player->LastWarn, but worth enough to cause warning level go up
void RajhCheatDetector::AddWarning(CPlayer * Player, int amount)
{
    if(Player->Server()->Tick() - Player->LastWarn < Player->Server()->TickSpeed() * 0.2)
    {
        str_format(aBuf, sizeof(aBuf), "'%s' got last warnings less than 200ms ago, ignoring amount of %d", Player->Server()->ClientName(Player->GetCID()), amount);
    }
    else
    {
        Player->Warnings += amount;
        Player->LastWarn = Player->Server()->Tick();

        str_format(aBuf, sizeof(aBuf), "'%s' warnings: %d", Player->Server()->ClientName(Player->GetCID()), Player->Warnings);
    }
    
	Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "rcd", aBuf);
}

void RajhCheatDetector::CheckWarnings(CPlayer * Player)
{
	if(Player->Warnings > 0 && Player->Server()->Tick() - Player->LastWarn > Player->Server()->TickSpeed() * 30)
	{
		Player->Warnings--;
		Player->LastWarn = Player->Server()->Tick();
		str_format(aBuf, sizeof(aBuf), "'%s' warnings: %d (30 sec without strange behavior)", Player->Server()->ClientName(Player->GetCID()), Player->Warnings);
		Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "rcd", aBuf);
	}

	if(g_Config.m_RcdEnable && Player->Warnings >= g_Config.m_RcdMaxWarnings && Player->Warnings > 0)
	{
		char aCmd[128] = {0};

		if (g_Config.m_RcdBantime == 0)
		{
			str_format(aCmd, sizeof(aCmd), "kick %d %s", Player->GetCID(), g_Config.m_RcdBanreason);
		}
		else
		{
			char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
			Player->Server()->GetClientAddr(Player->GetCID(), aAddrStr, sizeof(aAddrStr));
			str_format(aCmd, sizeof(aCmd), "ban %s %d %s", aAddrStr, g_Config.m_RcdBantime, g_Config.m_RcdBanreason);
		}

		Player->GameServer()->Console()->ExecuteLine(aCmd);
	}
}

bool RajhCheatDetector::CheckSomethingElse(CPlayer* Player, int Victim)
{
    #include "thing.h"
    
    return false;
}


bool RajhCheatDetector::CheckInputPos(CPlayer *Player, int Victim, warning_t& warnLevelOut)
{
	const float WhatICallClose = 8.f;

	CCharacter *CPlayer;
	CCharacter *CVictim;

	if(!(CPlayer = Player->GameServer()->GetPlayerChar(Player->GetCID())) || !(CVictim = Player->GameServer()->GetPlayerChar(Victim)))
		return false;

	vec2 Target = vec2(CPlayer->m_LatestInput.m_TargetX, CPlayer->m_LatestInput.m_TargetY);
	vec2 TargetPos = CPlayer->m_Pos + Target;

	float DistanceAimToVictim = distance(TargetPos, CVictim->m_Pos);

	// Probably not aim-bot if distance between target and victim >= 8
	// Ping may fake this
	if(DistanceAimToVictim >= WhatICallClose)
		return false;

	/* creates a dead zone at <= 50, uncomment that for now. if necessary will be superseeded by the check below
	// Ignore if distance between target and player <= 50
	// cl_mouse_max_distance <= 50 can cause false positives
	if(distance(TargetPos, CPlayer->m_Pos) <= 50.f) {
		str_format(aBuf, sizeof(aBuf), "'%s' aimed at '%s' position from close distance, ignoring", Player->Server()->ClientName(Player->GetCID()), Player->Server()->ClientName(Victim));
		Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "rcd", aBuf);

		warnLevelOut = 0;
		return true;
	}*/

	// This might become necessary in the future, see my explanation in PR
	float interpolatedMouseMaxDist = Player->MouseMaxDist;
	float aimDistance = distance(TargetPos, CPlayer->m_Pos);
	const int Tolerance = 2;
	if(interpolatedMouseMaxDist-Tolerance <= aimDistance && aimDistance <= interpolatedMouseMaxDist+Tolerance)
	{
	  str_format(aBuf, sizeof(aBuf), "'%s' aimed at mouse_max_dist +- %d, ignoring (cl_mouse_max_distance == %f)", Player->Server()->ClientName(Player->GetCID()), Tolerance, interpolatedMouseMaxDist);
	  Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "rcd", aBuf);

	  warnLevelOut = 0;
	  return true;
	}

	// Ignore shoots at non-moving target
	// Newbies can be accidentally banned for shooting afk players because of placing cursor on them directly.
	if(CVictim->m_LatestInput.m_Direction == 0 && CVictim->m_LatestInput.m_Jump == 0) {
		str_format(aBuf, sizeof(aBuf), "'%s' aimed at non-moving '%s', ignoring", Player->Server()->ClientName(Player->GetCID()), Player->Server()->ClientName(Victim));
		Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "rcd", aBuf);

		warnLevelOut = 0;
		return true;
	}

	str_format(aBuf, sizeof(aBuf), "'%s' aimed exactly at '%s' position (dist(TargetPos,Victim)==%f) ; (dist(TargetPos,Player)==%f)", Player->Server()->ClientName(Player->GetCID()), Player->Server()->ClientName(Victim), DistanceAimToVictim, aimDistance);
	Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "rcd", aBuf);

	// if DistanceAimToVictim == 1, player will get 7 warnings
	// if DistanceAimToVictim == 7, player will get 1 warning
	warnLevelOut = WhatICallClose - DistanceAimToVictim;

	return true;
}

bool RajhCheatDetector::CheckReflex(CPlayer * Player, int Victim)
{
			 CCharacter *CPlayer;
			 CCharacter *CVictim;
			 if(!(CPlayer = Player->GameServer()->GetPlayerChar(Player->GetCID())) || !(CVictim = Player->GameServer()->GetPlayerChar(Victim)))
				 return false;

			 if(distance(CPlayer->m_Pos, CVictim->m_Pos) < Player->GameServer()->m_World.m_Core.m_Tuning.m_LaserReach+5 &&
		distance(CPlayer->m_Pos, CVictim->m_Pos) > Player->GameServer()->m_World.m_Core.m_Tuning.m_LaserReach-5)
			 {
				 str_format(aBuf, sizeof(aBuf), "'%s' aimed exactly at '%s' at max range",Player->Server()->ClientName(Player->GetCID()), Player->Server()->ClientName(Victim));
// 				 Player->GameServer()->SendChat(-1,CGameContext::CHAT_ALL,aBuf);
				 Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "rcd", aBuf);
				 return true;
			 }
			 return false;
}

// adapted from CCharacter::CountInput()
static int TestFire(int prev, int cur)
{
			 prev &= INPUT_STATE_MASK;
			 cur &= INPUT_STATE_MASK;
			 int i = prev;
			 while(i != cur)
			 {
				 i = (i+1)&INPUT_STATE_MASK;
				 if(i&1)
					 return 1;
			 }

			 return 0;
}

bool RajhCheatDetector::CheckFastFire(CPlayer * Player)
{
	CCharacter *c = Player->GameServer()->GetPlayerChar(Player->GetCID());

	if(!c || !Player || !TestFire(c->m_LatestPrevInput.m_Fire, c->m_LatestInput.m_Fire))
		return false;

	// we ve collected enough samples
	if(Player->LastFireIdx >= Player->LastFireTick.size())
	{
		Player->LastFireIdx = 0;

		// derive to get the time diff between each fireing
		for(unsigned int i=0; i<Player->LastFireTick.size()-1; i++)
		{
			Player->LastFireTick[i] = Player->LastFireTick[i+1] - Player->LastFireTick[i];
		}
		unsigned int last = Player->LastFireTick.size()-1;
		Player->LastFireTick[last] = Player->Server()->Tick() - Player->LastFireTick[last];

		if(std::abs(Player->LastFireTick.sum()) <= 2 * Player->LastFireTick.size())
		{
			str_format(aBuf, sizeof(aBuf), "'%s' fires way too fast",Player->Server()->ClientName(Player->GetCID()));
			Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "rcd", aBuf);
                        
                        str_format(aBuf, sizeof(aBuf), "lastfiretick array for player id %d", Player->GetCID());
                        Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "rcd", aBuf);
                        
                        aBuf[0] = '\0';                        
                        char digBuf[10];   
                        for (unsigned int i=0; i<Player->LastFireTick.size(); i++)
                        {
                            str_format(digBuf, sizeof(digBuf), "%d ", Player->LastFireTick[i]);
                            strncat(aBuf, digBuf, 9);
                        }
                        Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "rcd", aBuf);
                        
			return true;
		}

		return false;
	}

	Player->LastFireTick[Player->LastFireIdx++] = Player->Server()->Tick();
	return false;
}
