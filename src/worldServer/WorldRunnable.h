#ifndef __WORLDRUNNABLE_H
#define __WORLDRUNNABLE_H

#include <Common.h>
#include <Threading.h>

/// Heartbeat thread for the World
class WorldRunnable : public Origin::Runnable
{
public:
	void run() override;
};
#endif
/// @}
