#ifndef ENGINE_SHARED_DNSBL_H
#define ENGINE_SHARED_DNSBL_H

#include <ares.h>
#include <arpa/nameser.h>
#include <base/system.h>
#include <engine/shared/jobs.h>

struct CDnsBlQuery
{
	NETADDR m_Addr;
	char m_Query[NETADDR_MAXSTRSIZE];
	char m_Server[NETADDR_MAXSTRSIZE];
	char *m_Reason;
	int m_Status;
};

enum
{
	MAX_BL_SERVERS=16,
	MAX_QUEUE_LENGTH=64,
};

class CDnsBl
{
protected:

	class IConsole *m_pConsole;
	class IStorage *m_pStorage;

	class CNetBan *m_pNetBan;

	const char* m_BlServers[MAX_BL_SERVERS];
	int m_BlServersCount;

	ares_channel m_AresChannel;

	pthread_mutex_t m_QueryThreadLock;
	pthread_cond_t m_QueryThreadCond;

	CDnsBlQuery m_Queue[MAX_QUEUE_LENGTH];
	int m_QueueHead;
	int m_QueueTail;

	class IConsole *Console() const { return m_pConsole; }
	class IStorage *Storage() const { return m_pStorage; }
	class CNetBan *NetBan() const { return m_pNetBan; }

	static void QueryThread(void *pUser);
	static void QueryCallback(void *pUser, int Status, int Timeouts, unsigned char *pBuf, int BufferSize);

	CDnsBlQuery CreateQuery(NETADDR *pAddr, const char* pBlServer);

public:

	void Init(class IConsole *pConsole, class IStorage *pStorage, CNetBan *pNetBan);

	static void ConAddDnsbl(IConsole::IResult *pResult, void *pUser);
	static void ConDnsbl(IConsole::IResult *pResult, void *pUser);
	static void ConClearDnsbl(IConsole::IResult *pResult, void *pUser);

	int AddServer(const char *pAddrStr);
	void CheckAndBan(NETADDR *pAddr);
};

#endif
