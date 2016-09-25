#include <iostream>
#include <engine/engine.h>
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

	m_BlServersCount = 0;
	m_QueueHead = m_QueueTail = 0;

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

	pthread_mutex_init(&m_QueryThreadLock, NULL);
	pthread_cond_init(&m_QueryThreadCond, NULL);

	thread_init(&QueryThread, this);

	Console()->Register("add_dnsbl", "s", CFGFLAG_SERVER|CFGFLAG_MASTER|CFGFLAG_STORE, ConAddDnsbl, this, "Add DNSBL server");
	Console()->Register("clear_dnsbl", "", CFGFLAG_SERVER|CFGFLAG_MASTER|CFGFLAG_STORE, ConClearDnsbl, this, "Clear DNSBL servers");
	Console()->Register("dnsbl", "", CFGFLAG_SERVER|CFGFLAG_MASTER|CFGFLAG_STORE, ConDnsbl, this, "");
}

void CDnsBl::ConAddDnsbl(IConsole::IResult *pResult, void *pUser)
{
	CDnsBl *pThis = static_cast<CDnsBl *>(pUser);

	const char *pAddrStr = pResult->GetString(0);

	if (pThis->AddServer(pAddrStr) == 1)
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "dnsbl", "too many DNSBL servers");
}

void CDnsBl::ConClearDnsbl(IConsole::IResult *pResult, void *pUser)
{
	CDnsBl *pThis = static_cast<CDnsBl *>(pUser);

	pThis->m_BlServersCount = 0;
	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "dnsbl", "cleared DNSBL servers");
}

void CDnsBl::ConDnsbl(IConsole::IResult *pResult, void *pUser)
{
	CDnsBl *pThis = static_cast<CDnsBl *>(pUser);

	for(int i = 0; i < pThis->m_BlServersCount; i++)
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "dnsbl", pThis->m_BlServers[i]);
}

int CDnsBl::AddServer(const char *pAddrStr)
{
	if (m_BlServersCount == MAX_BL_SERVERS)
		return 1;

	m_BlServers[m_BlServersCount] = pAddrStr;
	m_BlServersCount++;

	return 0;
}

void CDnsBl::QueryThread(void *pUser)
{
	dbg_msg("dnsbl", "query thread started");

	CDnsBl *pThis = static_cast<CDnsBl *>(pUser);

	int nfds;
	fd_set read_fds, write_fds;
	struct timeval tv;

	while (1) {
		pthread_cond_wait(&pThis->m_QueryThreadCond, &pThis->m_QueryThreadLock);
		while (pThis->m_QueueHead != pThis->m_QueueTail) {
			CDnsBlQuery *pQuery = &pThis->m_Queue[pThis->m_QueueTail];
			pThis->m_QueueTail = (pThis->m_QueueTail + 1) % MAX_QUEUE_LENGTH;
			pthread_mutex_unlock(&pThis->m_QueryThreadLock);

			pQuery->m_Status = -1;
			ares_query(pThis->m_AresChannel, pQuery->m_Query, ns_c_in, ns_t_txt, &QueryCallback, pQuery);

			// resolve query
			while (pQuery->m_Status == -1) {
				FD_ZERO(&read_fds);
				FD_ZERO(&write_fds);
				nfds = ares_fds(pThis->m_AresChannel, &read_fds, &write_fds);
				ares_timeout(pThis->m_AresChannel, NULL, &tv);
				select(nfds, &read_fds, &write_fds, NULL, &tv);
				ares_process(pThis->m_AresChannel, &read_fds, &write_fds);
			}

			// not blacklisted
			if (pQuery->m_Status == ARES_ENOTFOUND)
				continue;

			// error
			if (pQuery->m_Status != ARES_SUCCESS) {
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "resolve error: %s", ares_strerror(pQuery->m_Status));
				pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "dnsbl", aBuf);
				continue;
			}

			// ban IP
			char aAddrStr[NETADDR_MAXSTRSIZE];
			char aBanMsg[256];

			net_addr_str(&pQuery->m_Addr, aAddrStr, sizeof(aAddrStr), false);
			dbg_msg("dnsbl", "%s blocked by %s (%s)", aAddrStr, pQuery->m_Server, pQuery->m_Reason);

			if (!pThis->m_pNetBan->IsBanned(&pQuery->m_Addr, aBanMsg, sizeof(aBanMsg)))
				pThis->m_pNetBan->BanAddr(&pQuery->m_Addr, g_Config.m_DnsblBantime*60, pQuery->m_Reason);
		}
	}
}

void CDnsBl::QueryCallback(void *pUser, int Status, int Timeouts, unsigned char *pBuf, int BufferSize)
{
	CDnsBlQuery *pQuery = (CDnsBlQuery *)pUser;

	pQuery->m_Status = Status;

	if (Status != ARES_SUCCESS)
		return;

	// try to parse reason
	struct ares_txt_reply *Reply;
	Status = ares_parse_txt_reply(pBuf, BufferSize, &Reply);

	if (Status != ARES_SUCCESS) {
		pQuery->m_Status = Status;
		return;
	}

	pQuery->m_Reason = (char*)Reply->txt;
}

CDnsBlQuery CDnsBl::CreateQuery(NETADDR *pAddr, const char* pBlServer)
{
	char aPrtStr[NETADDR_MAXSTRSIZE];
	// str_copy(aPrtStr, "2.0.0.127", sizeof(aPrtStr));
	net_addr_ptr(pAddr, aPrtStr, sizeof(aPrtStr));

	CDnsBlQuery s_Query;

	s_Query.m_Addr.type = pAddr->type;
	mem_copy(s_Query.m_Addr.ip, pAddr->ip, sizeof(s_Query.m_Addr.ip));

	char aQuery[NETADDR_MAXSTRSIZE];
	str_format(s_Query.m_Query, sizeof(aQuery), "%s.%s", aPrtStr, pBlServer);

	str_copy(s_Query.m_Server, pBlServer, sizeof(s_Query.m_Server));

	return s_Query;
}

void CDnsBl::CheckAndBan(NETADDR *pAddr)
{
	for(int i = 0; i < m_BlServersCount; i++) {
		CDnsBlQuery s_Query = CreateQuery(pAddr, m_BlServers[i]);

		if (((m_QueueHead + 1) % MAX_QUEUE_LENGTH) == m_QueueTail) {
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "can't resolve %s: queue is full", s_Query.m_Query);
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "dnsbl", aBuf);
			continue;
		}

		pthread_mutex_lock(&m_QueryThreadLock);

		m_Queue[m_QueueHead] = s_Query;
		m_QueueHead = (m_QueueHead + 1) % MAX_QUEUE_LENGTH;

		pthread_mutex_unlock(&m_QueryThreadLock);
		pthread_cond_signal(&m_QueryThreadCond);
	}
}
