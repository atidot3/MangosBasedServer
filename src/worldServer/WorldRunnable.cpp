#include <Common.h>
#include "World.h"
#include "WorldRunnable.h"
#include <Timer.h>
//#include "ObjectAccessor.h"

#include <Database/DatabaseEnv.h>

#define WORLD_SLEEP_CONST 50

#ifdef WIN32
#include "ServiceWin32.h"
extern int m_ServiceStatus;
#endif

/// Heartbeat for the World
void WorldRunnable::run()
{
	///- Init new SQL thread for the world database
	WorldDatabase.ThreadStart();                            // let thread do safe mySQL requests (one connection call enough)
	//sWorld.InitResultQueue();

	uint32 realCurrTime = 0;
	uint32 realPrevTime = WorldTimer::tick();

	uint32 prevSleepTime = 0;                               // used for balanced full tick time length near WORLD_SLEEP_CONST

															///- While we have not World::m_stopEvent, update the world
	while (!World::IsStopped())
	{
		++World::m_worldLoopCounter;
		realCurrTime = WorldTimer::getMSTime();

		uint32 diff = WorldTimer::tick();

		sWorld.Update(diff);
		realPrevTime = realCurrTime;

		// diff (D0) include time of previous sleep (d0) + tick time (t0)
		// we want that next d1 + t1 == WORLD_SLEEP_CONST
		// we can't know next t1 and then can use (t0 + d1) == WORLD_SLEEP_CONST requirement
		// d1 = WORLD_SLEEP_CONST - t0 = WORLD_SLEEP_CONST - (D0 - d0) = WORLD_SLEEP_CONST + d0 - D0
		if (diff <= WORLD_SLEEP_CONST + prevSleepTime)
		{
			prevSleepTime = WORLD_SLEEP_CONST + prevSleepTime - diff;
			Origin::Thread::Sleep(prevSleepTime);
		}
		else
			prevSleepTime = 0;

		Sleep(1000);
	}
	sWorld.CleanupsBeforeStop();

															///- End the database thread
	WorldDatabase.ThreadEnd();                              // free mySQL thread resources
}
