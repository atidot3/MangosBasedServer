#ifndef _MASTER_H
#define _MASTER_H

#include <Common.h>
#include <Config/Singleton.h>
#include <Database\DatabaseEnv.h>

/// Start the server
class Master
{
public:
	int Run();
	static volatile uint32 m_masterLoopCounter;

private:
	bool _StartDB();
	void clearOnlineAccounts();
};

#define sMaster Origin::Singleton<Master>::Instance()
#endif