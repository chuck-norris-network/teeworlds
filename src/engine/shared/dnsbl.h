#ifndef ENGINE_SHARED_DNSBL_H
#define ENGINE_SHARED_DNSBL_H

#include <ares.h>
#include <base/system.h>
#include <arpa/nameser.h>

struct CQueryData
{
	class CDnsBl *m_DnsBl;
	NETADDR *m_pAddr;
};

class CDnsBl
{

	enum
	{
		MAX_BL_SERVERS=16,
	};

private:

	class IConsole *m_pConsole;
	class IStorage *m_pStorage;

	class CNetBan *m_pNetBan;

	ares_channel m_AresChannel;

	const char* m_BlServers[MAX_BL_SERVERS];
	int m_NumBlServers;

	class IConsole *Console() const { return m_pConsole; }
	class IStorage *Storage() const { return m_pStorage; }
	class CNetBan *NetBan() const { return m_pNetBan; }

	static void WaitQuery(ares_channel channel);
	static void QueryCallback(void *pUser, int Status, int Timeouts, unsigned char *pBuf, int BufferSize);

public:

	void Init(class IConsole *pConsole, class IStorage *pStorage, CNetBan *pNetBan);

	static void ConAddServer(IConsole::IResult *pResult, void *pUser);
	void AddServer(const char *pAddrStr);

	void CheckAndBan(NETADDR *pAddr);

};

#endif
