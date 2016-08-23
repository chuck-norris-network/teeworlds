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

	struct ares_options AresOptions;
	int AresOptMask = 0;

	if (ares_library_init(ARES_LIB_INIT_ALL) != ARES_SUCCESS) {
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "dnsbl", "can't initialize c-ares");
		return;
	}

	if(ares_init_options(&m_AresChannel, &AresOptions, AresOptMask) != ARES_SUCCESS) {
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "dnsbl", "can't initialize c-ares options");
		return;
	}

	Console()->Register("add_dnsbl", "s", CFGFLAG_SERVER|CFGFLAG_MASTER|CFGFLAG_STORE, ConAddServer, this, "Add DNSBL server");
	// Console()->Register("banmasters", "", CFGFLAG_SERVER, ConBanmasters, this, "");
	// Console()->Register("clear_banmasters", "", CFGFLAG_SERVER, ConClearBanmasters, this, "");
}

void CDnsBl::ConAddServer(IConsole::IResult *pResult, void *pUser)
{
	CDnsBl *pThis = static_cast<CDnsBl *>(pUser);

	const char *pAddrStr = pResult->GetString(0);

	pThis->AddServer(pAddrStr);

	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "dnsbl", "added new server");
}

void CDnsBl::AddServer(const char *pAddrStr)
{
	m_BlServers[m_NumBlServers] = pAddrStr;
	m_NumBlServers++;
}

void CDnsBl::WaitQuery(ares_channel channel)
{
	struct timeval *tvp, tv;
	fd_set read_fds, write_fds;
	int nfds;

	for (;;)
	{
		FD_ZERO(&read_fds);
		FD_ZERO(&write_fds);
		nfds = ares_fds(channel, &read_fds, &write_fds);
		if(nfds == 0){
				break;
		}
		tvp = ares_timeout(channel, NULL, &tv);
		select(nfds, &read_fds, &write_fds, NULL, tvp);
		ares_process(channel, &read_fds, &write_fds);
	}
}

void CDnsBl::QueryCallback(void *pUserData, int Status, int Timeouts, unsigned char *pBuf, int BufferSize)
{
	CQueryData *pQueryData = (CQueryData *)pUserData;

	if (Status != ARES_SUCCESS)
		return;

	struct ares_txt_reply *Reply;

	Status = ares_parse_txt_reply(pBuf, BufferSize, &Reply);
	if (Status != ARES_SUCCESS) {
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "status in parse: %s", ares_strerror(Status));
		pQueryData->m_DnsBl->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "dnsbl", aBuf);
		return;
	}

	char aIpStr[16];
	net_addr_str(pQueryData->m_pAddr, aIpStr, sizeof(aIpStr), false);
	dbg_msg("dnsbl", "%s blocked by DNSBL (%s)", aIpStr, Reply->txt);

	pQueryData->m_DnsBl->m_pNetBan->BanAddr(pQueryData->m_pAddr, g_Config.m_DnsBlBantime*60, (const char *)Reply->txt);
}

void CDnsBl::CheckAndBan(NETADDR *pAddr)
{
	char aPrtStr[NETADDR_MAXSTRSIZE];
	net_addr_ptr(pAddr, aPrtStr, sizeof(aPrtStr));

	for(int i = 0; i < m_NumBlServers; i++)
	{
		char Query[256];
		str_format(Query, sizeof(Query), "%s.%s", aPrtStr, m_BlServers[i]);

		static CQueryData s_QueryData = { this, pAddr };

		ares_query(m_AresChannel, Query, ns_c_in, ns_t_txt, QueryCallback, (void *)&s_QueryData);
		WaitQuery(m_AresChannel);
	}
}