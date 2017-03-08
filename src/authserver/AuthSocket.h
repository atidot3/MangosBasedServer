#ifndef _AUTHSOCKET_H
#define _AUTHSOCKET_H

#include <Common.h>
#include <BigNumber.h>
#include <Sha1.h>

#include <ByteBuffer.h>
#include <Scoket.h>
#include "AuthCodes.h"
#include "RealmList.h"

#include <boost/asio.hpp>
#include <functional>

struct REALM_RESULT;

class AuthSocket : public Origin::Socket
{
public:
	const static int s_BYTE_SIZE = 32;

	AuthSocket(boost::asio::io_service &service, std::function<void(Socket *)> closeHandler);

	void LoadRealmlist(REALM_RESULT& realm, uint32 acctid);
	bool _HandleOnLogin();
	bool _HandleRealmList();

private:
	bool _authed;

	std::string _login;
	std::string _safelogin;

	// Since GetLocaleByName() is _NOT_ bijective, we have to store the locale as a string. Otherwise we can't differ
	// between enUS and enGB, which is important for the patch system
	std::string _localizationName;
	uint16 _build;
	AccountTypes _accountSecurityLevel;

	virtual bool ProcessIncomingData() override;
};
#endif
/// @}
