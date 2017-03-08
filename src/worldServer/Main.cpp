#include "Master.h"

#include <iostream>
#include <string>
#include <chrono>
#include <thread>

#include <boost/program_options.hpp>
#include <boost/version.hpp>

#include <openssl/opensslv.h>
#include <openssl/crypto.h>

#include <Common.h>
#include <Config/Singleton.h>
#include <Database\DatabaseEnv.h>
#include <Listener.h>
#include <Util.h>
#include <Config\Config.h>
#include "Log.h"

#define _WORLDSD_CONFIG			"world.conf"

DatabaseType WorldDatabase;                                 ///< Accessor to the world database
DatabaseType CharacterDatabase;                             ///< Accessor to the character database
DatabaseType LoginDatabase;                                 ///< Accessor to the realm/login database

uint32 realmID;                                             ///< Id of the realm

int main(int argc, char* argv[])
{
	if (!sConfig.SetSource(_WORLDSD_CONFIG))
	{
		sLog.outError("Could not find configuration file %s.", _WORLDSD_CONFIG);
		::system("PAUSE");
		return 1;
	}
	sLog.Initialize();
	sLog.InitColors(sConfig.GetStringDefault("LogColors"));
	sLog.outString("Using configuration file %s.", _WORLDSD_CONFIG);

	sLog.outString("%s (Library: %s)", OPENSSL_VERSION_TEXT, SSLeay_version(SSLEAY_VERSION));
	sLog.outString("Using Boost: %s", BOOST_LIB_VERSION);

	return sMaster.Run();
}