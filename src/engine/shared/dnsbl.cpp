#include <iostream>
#include <engine/console.h>
#include <engine/storage.h>
#include <engine/shared/config.h>

#include "dnsbl.h"
#include "netban.h"

void CDnsBl::Init(IConsole *pConsole, IStorage *pStorage, CNetBan *pNetBan)
{
	m_pConsole = pConsole;
	m_pStorage = pStorage;
	m_pNetBan = pNetBan;

	m_NumBlServers = 0;

	Console()->Register("add_dnsbl", "s", CFGFLAG_SERVER|CFGFLAG_MASTER|CFGFLAG_STORE, ConAddServer, this, "Add DNSBL server");
	// Console()->Register("banmasters", "", CFGFLAG_SERVER, ConBanmasters, this, "");
	// Console()->Register("clear_banmasters", "", CFGFLAG_SERVER, ConClearBanmasters, this, "");
}

void CDnsBl::ConAddServer(IConsole::IResult *pResult, void *pUser)
{
	CDnsBl *pThis = static_cast<CDnsBl *>(pUser);

	const char *pAddrStr = pResult->GetString(0);

	int Result = pThis->AddServer(pAddrStr);

	if(Result != 0)
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "dnsbl", "added new server");
	// else
	// 	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "dnsbl", "can't add server");
}

int CDnsBl::AddServer(const char *pAddrStr)
{
	m_BlServers[m_NumBlServers] = pAddrStr;
	m_NumBlServers++;

	return 0;
}

void CDnsBl::CheckAndBan(const NETADDR *pAddr) const
{
	if(pAddr->type != NETTYPE_IPV4)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "dnsbl", "only IPv4 addresses supported");
		return;
	}

	// convert to PTR format
	int a, b, c, d;
	char ip[16];
	char ptr[16];
  net_addr_str(pAddr, ip, 16, 0);
	sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d);
	sprintf(ptr, "%d.%d.%d.%d", d, c, b, a);

	for(int i = 0; i < m_NumBlServers; i++)
	{
    NETADDR AnswerAddr;
		char aQuery[128];
		str_format(aQuery, sizeof(aQuery), "%s.%s", ptr, m_BlServers[i]);
		// str_format(aQuery, sizeof(aQuery), "2.0.0.127.%s", m_BlServers[i]);

		if(net_host_lookup(aQuery, &AnswerAddr, NETTYPE_IPV4) == 0)
		{
			dbg_msg("dnsbl", "%s blocked by %s", ip, m_BlServers[i]);

			char Reason[256];
			int Seconds = 24*60*60;
			str_format(Reason, sizeof(Reason), "Blocked by %s", m_BlServers[i]);

			m_pNetBan->BanAddr(pAddr, Seconds, Reason);

			return;
		}
	}
}
