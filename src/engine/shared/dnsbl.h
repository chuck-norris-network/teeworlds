#ifndef ENGINE_SHARED_DNSBL_H
#define ENGINE_SHARED_DNSBL_H

#include <ares.h>
#include <arpa/nameser.h>
#include <base/system.h>
#include <engine/shared/jobs.h>

struct CQueryData
{
	class CDnsBl *m_pDnsBl;
	NETADDR *m_pAddr;
	char m_Server[256];
	char m_Query[256];
};

class CDnsBl
{

	enum
	{
		MAX_BL_SERVERS=16,
	};

protected:

	class IConsole *m_pConsole;
	class IStorage *m_pStorage;

	class CNetBan *m_pNetBan;

	const char* m_BlServers[MAX_BL_SERVERS];
	int m_NumBlServers;

	ares_channel m_AresChannel;

	class IConsole *Console() const { return m_pConsole; }
	class IStorage *Storage() const { return m_pStorage; }
	class CNetBan *NetBan() const { return m_pNetBan; }

	static void QueryThread(void *pUser);
	static void QueryCallback(void *pUser, int Status, int Timeouts, unsigned char *pBuf, int BufferSize);
	void MakeQuery(NETADDR *pAddr, const char* m_BlServer);

public:

	void Init(class IConsole *pConsole, class IStorage *pStorage, CNetBan *pNetBan);

	static void ConAddDnsbl(IConsole::IResult *pResult, void *pUser);
	static void ConDnsbl(IConsole::IResult *pResult, void *pUser);
	static void ConClearDnsbl(IConsole::IResult *pResult, void *pUser);

	int AddServer(const char *pAddrStr);
	void CheckAndBan(NETADDR *pAddr);

};

#endif
