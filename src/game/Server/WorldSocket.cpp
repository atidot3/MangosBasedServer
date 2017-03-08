#include "WorldSocket.h"
#include <Common.h>
#include <Util/Util.h>
#include "../World/World.h"
#include <Util\WorldPacket.h>
#include "SharedDefine.h"
#include <Util\ByteBuffer.h>
#include "Opcodes.h"
#include <Database/DatabaseEnv.h>
#include <Auth/Sha1.h>
#include "WorldSession.h"
#include <Log.h>


#include <chrono>
#include <functional>
#include <memory>

#include <boost/asio.hpp>

#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif
struct ServerPktHeader
{
	uint16 size;
	uint16 cmd;
};
#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

WorldSocket::WorldSocket(boost::asio::io_service &service, std::function<void(Socket *)> closeHandler)
	: Socket(service, closeHandler), m_lastPingTime(std::chrono::system_clock::time_point::min()), m_overSpeedPings(0),
	m_useExistingHeader(false), m_session(nullptr), m_seed(urand())
{}

void WorldSocket::SendPacket(const WorldPacket& pct, bool immediate)
{
	if (IsClosed())
		return;

	// Dump outgoing packet.
	//sLog.outWorldPacketDump(GetRemoteEndpoint().c_str(), pct.GetOpcode(), pct.GetOpcodeName(), pct, false);

	ServerPktHeader header;

	header.cmd = pct.GetOpcode();
	EndianConvert(header.cmd);

	header.size = static_cast<uint16>(pct.size() + 4);
	EndianConvertReverse(header.size);

	//m_crypt.EncryptSend(reinterpret_cast<uint8 *>(&header), sizeof(header));

	Write(reinterpret_cast<const char *>(&header), sizeof(header));
	if (header.cmd != MSG_MOVEMENT)
		sLog.outDebug("Packet id: '%d' size: '%d' sended", header.cmd, pct.size());
	if (!!pct.size())
	{
		Write(reinterpret_cast<const char *>(pct.contents()), pct.size());
	}

	if (immediate)
		ForceFlushOut();
}
/// CLIENT SOCKET HAS BEEN CONNECTED, ASK HIM TO LOGIN
bool WorldSocket::Open()
{
	if (!Socket::Open())
		return false;

	// Send startup packet.
	WorldPacket packet(SMSG_AUTH_CHALLENGE, 40);
	packet << m_seed;

	BigNumber seed1;
	seed1.SetRand(16 * 8);
	packet.append(seed1.AsByteArray(16), 16);               // new encryption seeds

	BigNumber seed2;
	seed2.SetRand(16 * 8);
	packet.append(seed2.AsByteArray(16), 16);               // new encryption seeds

	SendPacket(packet);

	return true;
}

bool WorldSocket::ProcessIncomingData()
{
	ClientPktHeader header;

	if (m_useExistingHeader)
	{
		m_useExistingHeader = false;
		header = m_existingHeader;

		ReadSkip(sizeof(ClientPktHeader));
	}
	else
	{
		if (!Read((char *)&header, sizeof(ClientPktHeader)))
		{
			errno = EBADMSG;
			return false;
		}

		//m_crypt.DecryptRecv((uint8 *)&header, sizeof(ClientPktHeader));
		EndianConvertReverse(header.size);
		EndianConvert(header.cmd);
	}

	// there must always be at least four bytes for the opcode,
	// and 0x2800 is the largest supported buffer in the client
	if ((header.size < 4) || (header.size > 0x2800) || (header.cmd >= NUM_MSG_TYPES))
	{
		if (header.size < 4)
			sLog.outError("header.size < 4");
		if (header.size > 0x2800)
			sLog.outError("header.size > 0x2800");
		if (header.cmd >= NUM_MSG_TYPES)
			sLog.outError("header.cmd >= NUM_MSG_TYPES");
		sLog.outError("WorldSocket::ProcessIncomingData: client sent malformed packet size = %u , cmd = %u", header.size, header.cmd);
		errno = EINVAL;
		return false;
	}

	// the minus four is because we've already read the four byte opcode value
	const uint16 validBytesRemaining = header.size - 4;

	// check if the client has told us that there is more data than there is
	if (validBytesRemaining > ReadLengthRemaining())
	{
		// we must preserve the decrypted header so as not to corrupt the crypto state, and to prevent duplicating work
		m_useExistingHeader = true;
		m_existingHeader = header;

		// we move the read pointer backward because it will be skipped again later.  this is a slight kludge, but to solve
		// it more elegantly would require introducing protocol awareness into the socket library, which we want to avoid
		ReadSkip(-static_cast<int>(sizeof(ClientPktHeader)));
		errno = EBADMSG;
		return false;
	}

	Opcodes x;
	const OpcodesList opcode = static_cast<OpcodesList>(header.cmd);

	if (IsClosed())
	{
		return false;
	}

	std::unique_ptr<WorldPacket> pct(new WorldPacket(opcode, validBytesRemaining));

	if (validBytesRemaining)
	{
		pct->append(InPeak(), validBytesRemaining);
		ReadSkip(validBytesRemaining);
	}
	if (opcode != MSG_MOVEMENT)
		sLog.outDetail("Opcodes: '%u'", opcode);
	try
	{
		switch (opcode)
		{
			case CMSG_AUTH_SESSION:
				if (m_session)
				{
					sLog.outError("WorldSocket::ProcessIncomingData: Player send CMSG_AUTH_SESSION again");
					return false;
				}
				return HandleAuthSession(*pct);
			case CMSG_PING:
				return HandlePing(*pct);
			case CMSG_KEEP_ALIVE:
				DEBUG_LOG("CMSG_KEEP_ALIVE ,size: " SIZEFMTD " ", pct->size());
				return true;
			default:
			{
				if (!m_session)
				{
					sLog.outError("WorldSocket::ProcessIncomingData: Client not authed opcode = %u", uint32(opcode));
					return false;
				}

				m_session->QueuePacket(std::move(pct));
				return true;
			}
		}
	}
	catch (ByteBufferException&)
	{
		sLog.outError("WorldSocket::ProcessIncomingData ByteBufferException occured while parsing an instant handled packet (opcode: %u) from client %s, accountid=%i.",
			opcode, GetRemoteAddress().c_str(), m_session ? m_session->GetAccountId() : -1);

		if (sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG))
		{
			DEBUG_LOG("Dumping error-causing packet:");
			pct->hexlike();
		}

		if (sWorld.getConfig(CONFIG_BOOL_KICK_PLAYER_ON_BAD_PACKET))
		{
			DETAIL_LOG("Disconnecting session [account id %i / address %s] for badly formatted packet.",
				m_session ? m_session->GetAccountId() : -1, GetRemoteAddress().c_str());

			return false;
		}
	}
	return true;
}
bool IsAcceptableClientBuild(uint32 build)
{
	int accepted_versions[] = EXPECTED_ORIGIN_CLIENT_BUILD;
	for (int i = 0; accepted_versions[i]; ++i)
		if (int(build) == accepted_versions[i])
			return true;

	return false;
}
bool WorldSocket::HandleAuthSession(WorldPacket &recvPacket)
{
	std::string account;
	std::string password;
	uint32 build, id;
	LocaleConstant locale;

	recvPacket >> account;
	recvPacket >> password;
	recvPacket >> (uint32)build;

	sLog.outDetail("%s", account);
	sLog.outDetail("%s", password);
	QueryResult* result =
		LoginDatabase.PQuery("SELECT "
			"id "                      //0
			"FROM account "
			"WHERE username = '%s' AND md5 = '%s'",
			account.c_str(),
			password.c_str());
	if (!result)
	{
		sLog.outError("WorldSocket::HandleAuthSession: Sent Auth Response (unknown account).");
		return false;
	}
	Field* fields = result->Fetch();

	id = fields[0].GetUInt32();
	time_t mutetime = 0;
	locale = LOCALE_enUS;

	delete result;

	if (!(m_session = new WorldSession(id, this, AccountTypes(AccountTypes::SEC_PLAYER), mutetime, locale)))
		return false;
	sWorld.AddSession(m_session);
	return true;
}

bool WorldSocket::HandlePing(WorldPacket &recvPacket)
{
	uint32 ping;
	uint32 latency;

	// Get the ping packet content
	recvPacket >> ping;
	recvPacket >> latency;

	if (m_lastPingTime == std::chrono::system_clock::time_point::min())
		m_lastPingTime = std::chrono::system_clock::now();              // for 1st ping
	else
	{
		auto now = std::chrono::system_clock::now();
		std::chrono::seconds seconds = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastPingTime);
		m_lastPingTime = now;

		if (seconds.count() < 27)
		{
			++m_overSpeedPings;

			const uint32 max_count = sWorld.getConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS);

			if (max_count && m_overSpeedPings > max_count)
			{
				if (m_session && m_session->GetSecurity() == SEC_PLAYER)
				{
					sLog.outError("WorldSocket::HandlePing: Player kicked for "
						"overspeeded pings address = %s",
						GetRemoteAddress().c_str());

					return false;
				}
			}
		}
		else
			m_overSpeedPings = 0;
	}

	// critical section
	{
		if (m_session)
		{
			m_session->SetLatency(latency);
			m_session->ResetClientTimeDelay();
		}
		else
		{
			sLog.outError("WorldSocket::HandlePing: peer sent CMSG_PING, "
				"but is not authenticated or got recently kicked,"
				" address = %s",
				GetRemoteAddress().c_str());
			return false;
		}
	}

	WorldPacket packet(SMSG_PONG, 4);
	packet << ping;
	SendPacket(packet, true);

	return true;
}
