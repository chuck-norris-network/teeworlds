#include <engine/shared/config.h>
#include <engine/server.h>

#include "player.h"
#include "gamecontext.h"
#include "entity.h"

#include "rcd.hpp"

#include <string>
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
	if(CheckFastFire(Player))
		AddWarning(Player, 1);
}

void RajhCheatDetector::OnHit(CPlayer * Player, int Victim)
{
	if(Player->GetCID() == Victim)
		return;

	if(CheckInputPos(Player, Victim))
		AddWarning(Player, 4);

	if(CheckReflex(Player, Victim))
		AddWarning(Player, 2);
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
		Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "rcd", aBuf);
	}
	else if(playerFound && clanFound) // very likely, he got a new ip
	{
		str_format(aBuf, sizeof(aBuf), "Welcome back: '%s' (name, clan match)",Player->Server()->ClientName(Player->GetCID()));
		Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "rcd", aBuf);
	}
	else if(playerFound) // well, maybe he got a new ip
	{
		str_format(aBuf, sizeof(aBuf), "Welcome back: '%s' (name match)",Player->Server()->ClientName(Player->GetCID()));
		Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "rcd", aBuf);
	}
	else if(ipFound) // time will show
	{
		str_format(aBuf, sizeof(aBuf), "Welcome back: '%s' (ip match)",Player->Server()->ClientName(Player->GetCID()));
		Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "rcd", aBuf);
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

void RajhCheatDetector::AddWarning(CPlayer * Player, int amount)
{
	Player->Warnings += amount;
	Player->LastWarn = Player->Server()->Tick();

	str_format(aBuf, sizeof(aBuf), "'%s' warnings: %d", Player->Server()->ClientName(Player->GetCID()), Player->Warnings);
	Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "rcd", aBuf);
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

bool RajhCheatDetector::CheckInputPos(CPlayer *Player, int Victim)
{
	CCharacter *CPlayer;
	CCharacter *CVictim;

	if(!(CPlayer = Player->GameServer()->GetPlayerChar(Player->GetCID())) || !(CVictim = Player->GameServer()->GetPlayerChar(Victim)))
		return false;

	vec2 Target = vec2(CPlayer->m_LatestInput.m_TargetX, CPlayer->m_LatestInput.m_TargetY);
	vec2 TargetPos = CPlayer->m_Pos + Target;

	// Ignore if distance between target and player <= 50
	// cl_mouse_max_distance <= 50 can cause false positives
	if(distance(TargetPos, CPlayer->m_Pos) <= 50.f)
		return false;

	// Ping may fake this
	if(distance(TargetPos, CVictim->m_Pos) < 8.f)
	{
		str_format(aBuf, sizeof(aBuf), "'%s' aimed exactly at '%s' position",Player->Server()->ClientName(Player->GetCID()), Player->Server()->ClientName(Victim));
		Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "rcd", aBuf);
		return true;
	}

	return false;
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
				 Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "rcd", aBuf);
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

		// derive again to get the change of the diffs
		for(unsigned int i=0; i<Player->LastFireTick.size()-1; i++)
		{
			Player->LastFireTick[i] = Player->LastFireTick[i+1] - Player->LastFireTick[i];
		}
		Player->LastFireTick[last] = 0;

		if(std::abs(Player->LastFireTick.sum()) <= 1)
		{
			str_format(aBuf, sizeof(aBuf), "'%s' fires way too regularly",Player->Server()->ClientName(Player->GetCID()));
			Player->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "rcd", aBuf);
			return true;
		}

		return false;
	}

	Player->LastFireTick[Player->LastFireIdx++] = Player->Server()->Tick();
	return false;
}
