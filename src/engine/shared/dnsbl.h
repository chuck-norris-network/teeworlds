#ifndef ENGINE_SHARED_DNSBL_H
#define ENGINE_SHARED_DNSBL_H

#include <base/system.h>

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

  const char* m_BlServers[MAX_BL_SERVERS];
  int m_NumBlServers;

	class IConsole *Console() const { return m_pConsole; }
	class IStorage *Storage() const { return m_pStorage; }
	class CNetBan *NetBan() const { return m_pNetBan; }

public:

  void Init(class IConsole *pConsole, class IStorage *pStorage, CNetBan *pNetBan);

  static void ConAddServer(IConsole::IResult *pResult, void *pUser);
  int AddServer(const char *pAddrStr);
  void CheckAndBan(const NETADDR *pAddr) const;

};

#endif
