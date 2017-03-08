#ifndef _WORLDSOCKET_H
#define _WORLDSOCKET_H

#include <Common.h>
#include <Auth/AuthCrypt.h>
#include <Auth/BigNumber.h>
#include <Network/Scoket.h>

#include <chrono>
#include <functional>

class WorldPacket;
class WorldSession;


class WorldSocket : public Origin::Socket
{
private:
#pragma pack(push,1)
struct ClientPktHeader
{
	uint16 size;
	uint32 cmd;
};
#pragma pack(pop)

	/// Time in which the last ping was received
	std::chrono::system_clock::time_point m_lastPingTime;

	/// Keep track of over-speed pings ,to prevent ping flood.
	uint32 m_overSpeedPings;

	ClientPktHeader m_existingHeader;
	bool m_useExistingHeader;

	/// Class used for managing encryption of the headers
	AuthCrypt m_crypt;

	/// Session to which received packets are routed
	WorldSession *m_session;
	bool m_sessionFinalized;

	const uint32 m_seed;

	BigNumber m_s;

	/// process one incoming packet.
	virtual bool ProcessIncomingData() override;

	/// Called by ProcessIncoming() on CMSG_AUTH_SESSION.
	bool HandleAuthSession(WorldPacket &recvPacket);

	/// Called by ProcessIncoming() on CMSG_PING.
	bool HandlePing(WorldPacket &recvPacket);

public:
	WorldSocket(boost::asio::io_service &service, std::function<void(Socket *)> closeHandler);

	// send a packet \o/
	void SendPacket(const WorldPacket& pct, bool immediate = false);

	void ClearSession() { m_session = nullptr; }

	virtual bool Open() override;
	virtual bool Deletable() const override { return !m_session && Socket::Deletable(); }

	/// Return the session key
	BigNumber &GetSessionKey() { return m_s; }

};

#endif  /* _WORLDSOCKET_H */

/// @}
