#include "Master.h"
#include "WorldRunnable.h"
#include "CliRunnable.h"

#include <Timer.h>
#include <Util/Util.h>
#include <Log.h>
#include <Config\Config.h>
#include <Network/Listener.h>
#include <Network/Scoket.h>


#include <World\World.h>

INSTANTIATE_SINGLETON_1(Master);
volatile uint32 Master::m_masterLoopCounter = 0;

void signalHandler(int signum)
{
	sLog.outDebug("Interrupt signal (%d) received.\n", signum);
	switch (signum)
	{
		case SIGINT:
		{
			World::StopNow(SHUTDOWN_EXIT_CODE);
			break;
		}
		case SIGTERM:
		{
			World::StopNow(SHUTDOWN_EXIT_CODE);
			break;
		}
		case SIGBREAK:
		{
			World::StopNow(SHUTDOWN_EXIT_CODE);
			break;
		}
	}
}

std::string AcceptableClientBuildsListStr()
{
	std::ostringstream data;
	int accepted_versions[] = EXPECTED_ORIGIN_CLIENT_BUILD;
	for (int i = 0; accepted_versions[i]; ++i)
		data << accepted_versions[i] << " ";
	return data.str();
}

class FreezeDetectorRunnable : public Origin::Runnable
{
public:
	FreezeDetectorRunnable() { _delaytime = 0; }
	uint32 m_loops, m_lastchange;
	uint32 w_loops, w_lastchange;
	uint32 _delaytime;
	void SetDelayTime(uint32 t) { _delaytime = t; }
	void run(void)
	{
		if (!_delaytime)
			return;
		sLog.outString("Starting up anti-freeze thread (%u seconds max stuck time)...", _delaytime / 1000);
		m_loops = 0;
		w_loops = 0;
		m_lastchange = 0;
		w_lastchange = 0;
		while (!World::IsStopped())
		{
			Origin::Thread::Sleep(1000);

			uint32 curtime = WorldTimer::getMSTime();
			// DEBUG_LOG("anti-freeze: time=%u, counters=[%u; %u]",curtime,Master::m_masterLoopCounter,World::m_worldLoopCounter);
				
			// normal work
			if (w_loops != World::m_worldLoopCounter)
			{
				w_lastchange = curtime;
				w_loops = World::m_worldLoopCounter;
			}
			// possible freeze
			else if (WorldTimer::getMSTimeDiff(w_lastchange, curtime) > _delaytime)
			{
				sLog.outError("World Thread hangs, kicking out server!");
				*((uint32 volatile*)nullptr) = 0;          // bang crash
			}
		}
		sLog.outString("Anti-freeze thread exiting without problems.");
	}
};

/// Clear 'online' status for all accounts with characters in this realm
void Master::clearOnlineAccounts()
{
	// Cleanup online status for characters hosted at current realm
	/// \todo Only accounts with characters logged on *this* realm should have online status reset. Move the online column from 'account' to 'realmcharacters'?
	LoginDatabase.PExecute("UPDATE account SET active_realm_id = 0 WHERE active_realm_id = '%u'", realmID);

	CharacterDatabase.Execute("UPDATE characters SET online = 0 WHERE online<>0");

	// Battleground instance ids reset at server restart
	//CharacterDatabase.Execute("UPDATE character_battleground_data SET instance_id = 0");
}

int Master::Run()
{
	signal(SIGINT, signalHandler);
	signal(SIGABRT, signalHandler);
	signal(SIGFPE, signalHandler);
	signal(SIGILL, signalHandler);
	signal(SIGSEGV, signalHandler);
	signal(SIGTERM, signalHandler);
	signal(SIGBREAK, signalHandler);
	///- Start the databases
	if (!_StartDB())
	{
		sLog.outError("StartDB() return an error !");
		Log::WaitBeforeContinueIfNeed();
		return 1;
	}

	///- Initialize the World
	sWorld.SetInitialWorldSettings();

	CharacterDatabase.AllowAsyncTransactions();
	WorldDatabase.AllowAsyncTransactions();
	LoginDatabase.AllowAsyncTransactions();

	///- Launch WorldRunnable thread
	Origin::Thread world_thread(new WorldRunnable);
	world_thread.setPriority(Origin::Priority_Highest);
	// set realmbuilds depend on world expected builds, and set server online
	{
		std::string builds = AcceptableClientBuildsListStr();
		LoginDatabase.escape_string(builds);
		LoginDatabase.DirectPExecute("UPDATE realmlist SET realmflags = realmflags & ~(%u), population = 0, realmbuilds = '%s'  WHERE id = '%u'", REALM_FLAG_OFFLINE, builds.c_str(), realmID);
	}
	Origin::Thread* cliThread = nullptr;
	{
		///- Launch CliRunnable thread
		cliThread = new Origin::Thread(new CliRunnable);
	}
	///- Start up freeze catcher thread
	Origin::Thread* freeze_thread = nullptr;
	if (uint32 freeze_delay = sConfig.GetIntDefault("MaxCoreStuckTime", 0))
	{
		FreezeDetectorRunnable* fdr = new FreezeDetectorRunnable();
		fdr->SetDelayTime(freeze_delay * 1000);
		freeze_thread = new Origin::Thread(fdr);
		freeze_thread->setPriority(Origin::Priority_Highest);
	}
	{
		//auto const listenIP = sConfig.GetStringDefault("BindIP", "0.0.0.0");
		Origin::Listener<WorldSocket> listener(sWorld.getConfig(CONFIG_UINT32_PORT_WORLD), 8);

		/*std::unique_ptr<Origin::Listener<RASocket>> raListener;
		if (sConfig.GetBoolDefault("Ra.Enable", false))
			raListener.reset(new Origin::Listener<RASocket>(sConfig.GetIntDefault("Ra.Port", 3443), 1));

		std::unique_ptr<SOAPThread> soapThread;
		if (sConfig.GetBoolDefault("SOAP.Enabled", false))
			soapThread.reset(new SOAPThread("0.0.0.0", sConfig.GetIntDefault("SOAP.Port", 7878)));*/

		// wait for shut down and then let things go out of scope to close them down
		while (!World::IsStopped())
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}
	///- Stop freeze protection before shutdown tasks
	if (freeze_thread)
	{
		freeze_thread->destroy();
		delete freeze_thread;
	}
	///- Set server offline in realmlist
	LoginDatabase.DirectPExecute("UPDATE realmlist SET realmflags = realmflags | %u WHERE id = '%u'", REALM_FLAG_OFFLINE, realmID);

	// when the main thread closes the singletons get unloaded
	// since worldrunnable uses them, it will crash if unloaded after master
	world_thread.wait();

	///- Clean account database before leaving
	clearOnlineAccounts();

	///- Wait for DB delay threads to end
	CharacterDatabase.HaltDelayThread();
	WorldDatabase.HaltDelayThread();
	LoginDatabase.HaltDelayThread();

	sLog.outString("Halting process...");
	if (cliThread)
	{
#ifdef WIN32

		// this only way to terminate CLI thread exist at Win32 (alt. way exist only in Windows Vista API)
		//_exit(1);
		// send keyboard input to safely unblock the CLI thread
		INPUT_RECORD b[5];
		HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
		b[0].EventType = KEY_EVENT;
		b[0].Event.KeyEvent.bKeyDown = TRUE;
		b[0].Event.KeyEvent.uChar.AsciiChar = 'X';
		b[0].Event.KeyEvent.wVirtualKeyCode = 'X';
		b[0].Event.KeyEvent.wRepeatCount = 1;

		b[1].EventType = KEY_EVENT;
		b[1].Event.KeyEvent.bKeyDown = FALSE;
		b[1].Event.KeyEvent.uChar.AsciiChar = 'X';
		b[1].Event.KeyEvent.wVirtualKeyCode = 'X';
		b[1].Event.KeyEvent.wRepeatCount = 1;

		b[2].EventType = KEY_EVENT;
		b[2].Event.KeyEvent.bKeyDown = TRUE;
		b[2].Event.KeyEvent.dwControlKeyState = 0;
		b[2].Event.KeyEvent.uChar.AsciiChar = '\r';
		b[2].Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
		b[2].Event.KeyEvent.wRepeatCount = 1;
		b[2].Event.KeyEvent.wVirtualScanCode = 0x1c;

		b[3].EventType = KEY_EVENT;
		b[3].Event.KeyEvent.bKeyDown = FALSE;
		b[3].Event.KeyEvent.dwControlKeyState = 0;
		b[3].Event.KeyEvent.uChar.AsciiChar = '\r';
		b[3].Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
		b[3].Event.KeyEvent.wVirtualScanCode = 0x1c;
		b[3].Event.KeyEvent.wRepeatCount = 1;
		DWORD numb;
		BOOL ret = WriteConsoleInput(hStdIn, b, 4, &numb);

		cliThread->wait();
#else
		cliThread->destroy();
#endif
		delete cliThread;
	}
	return 0;
}
bool Master::_StartDB()
{
	///- Get world database info from configuration file
	std::string dbstring = sConfig.GetStringDefault("WorldDatabaseInfo");
	int nConnections = sConfig.GetIntDefault("WorldDatabaseConnections", 1);
	if (dbstring.empty())
	{
		sLog.outError("Database not specified in configuration file");
		::system("PAUSE");
		return false;
	}
	sLog.outString("World Database total connections: %i", nConnections + 1);

	///- Initialise the world database
	if (!WorldDatabase.Initialize(dbstring.c_str(), nConnections))
	{
		sLog.outError("Cannot connect to world database %s", dbstring.c_str());
		::system("PAUSE");
		return false;
	}

	/*if (!WorldDatabase.CheckRequiredField("db_version", 0))
	{
		///- Wait for already started DB delay threads to end
		sLog.outError("db_version mismatch !");
		WorldDatabase.HaltDelayThread();
		return false;
	}*/

	dbstring = sConfig.GetStringDefault("CharacterDatabaseInfo");
	nConnections = sConfig.GetIntDefault("CharacterDatabaseConnections", 1);
	if (dbstring.empty())
	{
		sLog.outError("Character Database not specified in configuration file");

		///- Wait for already started DB delay threads to end
		WorldDatabase.HaltDelayThread();
		::system("PAUSE");
		return false;
	}
	sLog.outString("Character Database total connections: %i", nConnections + 1);

	///- Initialise the Character database
	if (!CharacterDatabase.Initialize(dbstring.c_str(), nConnections))
	{
		sLog.outError("Cannot connect to Character database %s", dbstring.c_str());

		///- Wait for already started DB delay threads to end
		WorldDatabase.HaltDelayThread();
		::system("PAUSE");
		return false;
	}

	/*if (!CharacterDatabase.CheckRequiredField("character_db_version", 0))
	{
		///- Wait for already started DB delay threads to end
		sLog.outError("character_db_version mismatch !");
		WorldDatabase.HaltDelayThread();
		CharacterDatabase.HaltDelayThread();
		::system("PAUSE");
		return false;
	}*/

	///- Get login database info from configuration file
	dbstring = sConfig.GetStringDefault("LoginDatabaseInfo");
	nConnections = sConfig.GetIntDefault("LoginDatabaseConnections", 1);
	if (dbstring.empty())
	{
		sLog.outError("Login database not specified in configuration file");

		///- Wait for already started DB delay threads to end
		WorldDatabase.HaltDelayThread();
		CharacterDatabase.HaltDelayThread();
		::system("PAUSE");
		return false;
	}

	///- Initialise the login database
	sLog.outString("Login Database total connections: %i", nConnections + 1);
	if (!LoginDatabase.Initialize(dbstring.c_str(), nConnections))
	{
		sLog.outError("Cannot connect to login database %s", dbstring.c_str());

		///- Wait for already started DB delay threads to end
		WorldDatabase.HaltDelayThread();
		CharacterDatabase.HaltDelayThread();
		::system("PAUSE");
		return false;
	}

	/*if (!LoginDatabase.CheckRequiredField("realmd_db_version", 0))
	{
		///- Wait for already started DB delay threads to end
		sLog.outError("realmd_db_version mismatch !");
		WorldDatabase.HaltDelayThread();
		CharacterDatabase.HaltDelayThread();
		LoginDatabase.HaltDelayThread();
		::system("PAUSE");
		return false;
	}*/

	sLog.outString();

	///- Get the realm Id from the configuration file
	realmID = sConfig.GetIntDefault("RealmID", 0);
	if (!realmID)
	{
		sLog.outError("Realm ID not defined in configuration file");

		///- Wait for already started DB delay threads to end
		WorldDatabase.HaltDelayThread();
		CharacterDatabase.HaltDelayThread();
		LoginDatabase.HaltDelayThread();
		::system("PAUSE");
		return false;
	}

	sLog.outString("Realm running as realm ID %d", realmID);
	sLog.outString();

	///- Clean the database before starting
	clearOnlineAccounts();

	/*sWorld.LoadDBVersion();

	sLog.outString("Using World DB: %s", sWorld.GetDBVersion());
	sLog.outString("Using creature EventAI: %s", sWorld.GetCreatureEventAIVersion());
	sLog.outString();*/
	return true;
}