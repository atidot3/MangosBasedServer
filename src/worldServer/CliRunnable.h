#ifndef __CLIRUNNABLE_H
#define __CLIRUNNABLE_H

#include <Common.h>
#include <Threading.h>

/// Command Line Interface handling thread
class CliRunnable : public Origin::Runnable
{
public:
	void run() override;
};
#endif
/// @}
