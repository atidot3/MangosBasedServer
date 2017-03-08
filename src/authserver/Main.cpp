#include <program_options.hpp>
#include <version.hpp>

#include <iostream>
#include <string>
#include <chrono>
#include <thread>

#include <Database\DatabaseEnv.h>
#include <Listener.h>
#include <Util.h>
#include "AuthSocket.h"
#include <Config\Config.h>
#include "RealmList.h"
#include "Log.h"

#define _REALMSD_CONFIG			"realmd.conf"
bool StartDB();
void signalHandler(int signum);
bool stopEvent = false;                                     ///< Setting it to true stops the server

DatabaseType LoginDatabase;  


using namespace boost;


int main(int argc, char *argv[])
{
	signal(SIGINT, signalHandler);
	signal(SIGABRT, signalHandler);
	signal(SIGFPE, signalHandler);
	signal(SIGILL, signalHandler);
	signal(SIGSEGV, signalHandler);
	signal(SIGTERM, signalHandler);
	signal(SIGBREAK, signalHandler);


	if (!sConfig.SetSource(_REALMSD_CONFIG))
	{
		sLog.outError("Could not find configuration file %s.", _REALMSD_CONFIG);
		::system("PAUSE");
		return 1;
	}
	sLog.Initialize();
	sLog.InitColors(sConfig.GetStringDefault("LogColors"));
	sLog.outString("%s (Library: %s)\n", OPENSSL_VERSION_TEXT, SSLeay_version(SSLEAY_VERSION));

	/// realmd PID file creation
	std::string pidfile = sConfig.GetStringDefault("PidFile");
	if (!pidfile.empty())
	{
		uint32 pid = CreatePIDFile(pidfile);
		if (!pid)
		{
			sLog.outError("Cannot create PID file %s.", pidfile.c_str());
			::system("PAUSE");
			return 1;
		}
	}

	///- Initialize the database connection
	if (!StartDB())
	{
		::system("PAUSE");
		return 1;
	}
	// set realm etc
	sRealmList.Initialize(sConfig.GetIntDefault("RealmsStateUpdateDelay", 20));
	if (sRealmList.size() == 0)
	{
		sLog.outError("No valid realms specified.\n");
		::system("PAUSE");
		return 1;
	}
	// cleanup query
	LoginDatabase.BeginTransaction();
	LoginDatabase.Execute("UPDATE account_banned SET active = 0 WHERE unbandate<=UNIX_TIMESTAMP() AND unbandate<>bandate");
	LoginDatabase.Execute("DELETE FROM ip_banned WHERE unbandate<=UNIX_TIMESTAMP() AND unbandate<>bandate");
	LoginDatabase.CommitTransaction();


	auto rmport = sConfig.GetIntDefault("RealmServerPort", 12345);
	std::string bind_ip = sConfig.GetStringDefault("BindIP", "0.0.0.0");
	Origin::Listener<AuthSocket> listener(rmport, 1);

	auto const numLoops = sConfig.GetIntDefault("MaxPingTime", 30) * MINUTE * 10;
	uint32 loopCounter = 0;
	///- Wait for termination signal
	while (!stopEvent)
	{
		if ((++loopCounter) == numLoops)
		{
			loopCounter = 0;
			//printf("Ping MySQL to keep connection alive");
			LoginDatabase.Ping();
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	LoginDatabase.HaltDelayThread();
	sLog.outString("Halting process...");
	return 0;
}

bool StartDB()
{
	std::string dbstring = sConfig.GetStringDefault("LoginDatabaseInfo");
	if (dbstring.empty())
	{
		sLog.outError("Database not specified");
		return false;
	}

	sLog.outDebug("Login Database total connections: %i", 1 + 1);

	if (!LoginDatabase.Initialize(dbstring.c_str()))
	{
		sLog.outError("Cannot connect to database\n");
		return false;
	}

	if (!LoginDatabase.CheckRequiredField("realmd_db_version", 0))
	{
		///- Wait for already started DB delay threads to end
		sLog.outError("Required Field: realmd_db_version please update your database.");
		LoginDatabase.HaltDelayThread();
		return false;
	}
	return true;
}
void signalHandler(int signum)
{
	std::cout << "Interrupt signal (" << signum << ") received.\n" << std::endl;
	stopEvent = true;
}