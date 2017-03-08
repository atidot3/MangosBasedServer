#include "WorldSocket.h"                                    // must be first to make ACE happy with ACE includes in it
#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Player.h"
#include "../World/World.h"
#include "ObjectAccessor.h"

#include <mutex>
#include <deque>
#include <memory>

// select opcodes appropriate for processing in Map::Update context for current session state
static bool MapSessionFilterHelper(WorldSession* session, OpcodeHandler const& opHandle)
{
	// we do not process thread-unsafe packets
	if (opHandle.packetProcessing == PROCESS_THREADUNSAFE)
		return false;

	// we do not process not loggined player packets
	Player* plr = session->GetPlayer();
	if (!plr)
		return false;

	// in Map::Update() we do not process packets where player is not in world!
	return plr->IsInWorld();
	return false;
}

bool MapSessionFilter::Process(WorldPacket* packet)
{
	OpcodeHandler const& opHandle = opcodeTable[packet->GetOpcode()];
	if (opHandle.packetProcessing == PROCESS_INPLACE)
		return true;

	// let's check if our opcode can be really processed in Map::Update()
	return MapSessionFilterHelper(m_pSession, opHandle);
}

// we should process ALL packets when player is not in world/logged in
// OR packet handler is not thread-safe!
bool WorldSessionFilter::Process(WorldPacket* packet)
{
	OpcodeHandler const& opHandle = opcodeTable[packet->GetOpcode()];
	// check if packet handler is supposed to be safe
	if (opHandle.packetProcessing == PROCESS_INPLACE)
		return true;

	// let's check if our opcode can't be processed in Map::Update()
	return !MapSessionFilterHelper(m_pSession, opHandle);
}

/// WorldSession constructor
WorldSession::WorldSession(uint32 id, WorldSocket* sock, AccountTypes sec, time_t mute_time, LocaleConstant locale) :
	m_muteTime(mute_time),
	_player(nullptr), m_Socket(sock), _security(sec), _accountId(id), _logoutTime(0),
	m_inQueue(false), m_playerLoading(false), m_playerLogout(false), m_playerRecentlyLogout(false), m_playerSave(false),
	m_latency(0), m_clientTimeDelay(0), m_tutorialState(TUTORIALDATA_UNCHANGED) {}

/// WorldSession destructor
WorldSession::~WorldSession()
{
	///- unload player if not unloaded
	if (_player)
		LogoutPlayer(true);
	m_Socket->ClearSession();
}
/// Get the player name
char const* WorldSession::GetPlayerName() const
{
	return GetPlayer() ? GetPlayer()->GetName() : "<none>";
}
void WorldSession::SizeError(WorldPacket const& packet, uint32 size) const
{
	sLog.outError("Client (account %u) send packet %s (%u) with size " SIZEFMTD " but expected %u (attempt crash server?), skipped",
		GetAccountId(), packet.GetOpcodeName(), packet.GetOpcode(), packet.size(), size);
}

/// Send a packet to the client
void WorldSession::SendPacket(WorldPacket const* packet, bool immediate)
{
	if (m_Socket->IsClosed())
		return;

#ifdef ORIGIN_DEBUG

	// Code for network use statistic
	static uint64 sendPacketCount = 0;
	static uint64 sendPacketBytes = 0;

	static time_t firstTime = time(nullptr);
	static time_t lastTime = firstTime;                     // next 60 secs start time

	static uint64 sendLastPacketCount = 0;
	static uint64 sendLastPacketBytes = 0;

	time_t cur_time = time(nullptr);

	if ((cur_time - lastTime) < 60)
	{
		sendPacketCount += 1;
		sendPacketBytes += packet->size();

		sendLastPacketCount += 1;
		sendLastPacketBytes += packet->size();
	}
	else
	{
		uint64 minTime = uint64(cur_time - lastTime);
		uint64 fullTime = uint64(lastTime - firstTime);
		DETAIL_LOG("Send all time packets count: " UI64FMTD " bytes: " UI64FMTD " avr.count/sec: %f avr.bytes/sec: %f time: %u", sendPacketCount, sendPacketBytes, float(sendPacketCount) / fullTime, float(sendPacketBytes) / fullTime, uint32(fullTime));
		DETAIL_LOG("Send last min packets count: " UI64FMTD " bytes: " UI64FMTD " avr.count/sec: %f avr.bytes/sec: %f", sendLastPacketCount, sendLastPacketBytes, float(sendLastPacketCount) / minTime, float(sendLastPacketBytes) / minTime);

		lastTime = cur_time;
		sendLastPacketCount = 1;
		sendLastPacketBytes = packet->wpos();               // wpos is real written size
	}

#endif                                                  // !ORIGIN_DEBUG
	m_Socket->SendPacket(*packet, immediate);
}

/// Add an incoming packet to the queue
void WorldSession::QueuePacket(std::unique_ptr<WorldPacket> new_packet)
{
	std::lock_guard<std::mutex> guard(m_recvQueueLock);
	m_recvQueue.push_back(std::move(new_packet));
}

/// Logging helper for unexpected opcodes
void WorldSession::LogUnexpectedOpcode(WorldPacket const& packet, const char* reason)
{
	sLog.outError("SESSION: received unexpected opcode %s (0x%.4X) %s",
		packet.GetOpcodeName(),
		packet.GetOpcode(),
		reason);
}

/// Logging helper for unexpected opcodes
void WorldSession::LogUnprocessedTail(WorldPacket const& packet)
{
	sLog.outError("SESSION: opcode %s (0x%.4X) have unprocessed tail data (read stop at " SIZEFMTD " from " SIZEFMTD ")",
		packet.GetOpcodeName(),
		packet.GetOpcode(),
		packet.rpos(), packet.wpos());
}

/// Update the WorldSession (triggered by World update)
bool WorldSession::Update(PacketFilter& updater)
{
	std::lock_guard<std::mutex> guard(m_recvQueueLock);
	///- Retrieve packets from the receive queue and call the appropriate handlers
	/// not process packets if socket already closed
	while (m_Socket && !m_Socket->IsClosed() && !m_recvQueue.empty())
	{
		auto const packet = std::move(m_recvQueue.front());
		m_recvQueue.pop_front();

		/*#if 1
		sLog.outError( "MOEP: %s (0x%.4X)",
		packet->GetOpcodeName(),
		packet->GetOpcode());
		#endif*/
		OpcodeHandler const& opHandle = opcodeTable[packet->GetOpcode()];
		try
		{
			switch (opHandle.status)
			{
			case STATUS_LOGGEDIN:
				if (!_player)
				{
					// skip STATUS_LOGGEDIN opcode unexpected errors if player logout sometime ago - this can be network lag delayed packets
					if (!m_playerRecentlyLogout)
						LogUnexpectedOpcode(*packet, "the player has not logged in yet");
				}
				else if (_player->IsInWorld())
					ExecuteOpcode(opHandle, *packet);

				// lag can cause STATUS_LOGGEDIN opcodes to arrive after the player started a transfer
				break;
			case STATUS_LOGGEDIN_OR_RECENTLY_LOGGEDOUT:
				if (!_player && !m_playerRecentlyLogout)
				{
					LogUnexpectedOpcode(*packet, "the player has not logged in yet and not recently logout");
				}
				else
					// not expected _player or must checked in packet hanlder
					ExecuteOpcode(opHandle, *packet);
				break;
			case STATUS_TRANSFER:
				if (!_player)
					LogUnexpectedOpcode(*packet, "the player has not logged in yet");
				else if (_player->IsInWorld())
					LogUnexpectedOpcode(*packet, "the player is still in world");
				else
					ExecuteOpcode(opHandle, *packet);
				break;
			case STATUS_AUTHED:
				// prevent cheating with skip queue wait
				if (m_inQueue)
				{
					LogUnexpectedOpcode(*packet, "the player not pass queue yet");
					break;
				}

				// single from authed time opcodes send in to after logout time
				// and before other STATUS_LOGGEDIN_OR_RECENTLY_LOGGOUT opcodes.
				m_playerRecentlyLogout = false;

				ExecuteOpcode(opHandle, *packet);
				break;
			case STATUS_NEVER:
				sLog.outError("SESSION: received not allowed opcode %s (0x%.4X)",
					packet->GetOpcodeName(),
					packet->GetOpcode());
				break;
			case STATUS_UNHANDLED:
				DEBUG_LOG("SESSION: received not handled opcode %s (0x%.4X)",
					packet->GetOpcodeName(),
					packet->GetOpcode());
				break;
			default:
				sLog.outError("SESSION: received wrong-status-req opcode %s (0x%.4X)",
					packet->GetOpcodeName(),
					packet->GetOpcode());
				break;
			}
		}
		catch (ByteBufferException&)
		{
			sLog.outError("WorldSession::Update ByteBufferException occured while parsing a packet (opcode: %u) from client %s, accountid=%i.",
				packet->GetOpcode(), GetRemoteAddress().c_str(), GetAccountId());
			if (sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG))
			{
				DEBUG_LOG("Dumping error causing packet:");
				packet->hexlike();
			}

			if (sWorld.getConfig(CONFIG_BOOL_KICK_PLAYER_ON_BAD_PACKET))
			{
				DETAIL_LOG("Disconnecting session [account id %u / address %s] for badly formatted packet.",
					GetAccountId(), GetRemoteAddress().c_str());

				KickPlayer();
			}
		}
	}

	// check if we are safe to proceed with logout
	// logout procedure should happen only in World::UpdateSessions() method!!!
	if (updater.ProcessLogout())
	{
		///- If necessary, log the player out
		const time_t currTime = time(nullptr);

		if (m_Socket->IsClosed() || (ShouldLogOut(currTime) && !m_playerLoading))
			LogoutPlayer(true);

		// finalize the session if disconnected.
		if (m_Socket->IsClosed())
			return false;
	}

	return true;
}

void WorldSession::Handle_NULL(WorldPacket& recvPacket)
{
	DEBUG_LOG("SESSION: received unimplemented opcode %s (0x%.4X)",
		recvPacket.GetOpcodeName(),
		recvPacket.GetOpcode());
}

void WorldSession::Handle_EarlyProccess(WorldPacket& recvPacket)
{
	sLog.outError("SESSION: received opcode %s (0x%.4X) that must be processed in WorldSocket::OnRead",
		recvPacket.GetOpcodeName(),
		recvPacket.GetOpcode());
}

void WorldSession::Handle_ServerSide(WorldPacket& recvPacket)
{
	sLog.outError("SESSION: received server-side opcode %s (0x%.4X)",
		recvPacket.GetOpcodeName(),
		recvPacket.GetOpcode());
}

void WorldSession::Handle_Deprecated(WorldPacket& recvPacket)
{
	sLog.outError("SESSION: received deprecated opcode %s (0x%.4X)",
		recvPacket.GetOpcodeName(),
		recvPacket.GetOpcode());
}

void WorldSession::SendAuthWaitQue(uint32 position)
{
	if (position == 0)
	{
		WorldPacket packet(SMSG_AUTH_RESPONSE, 1);
		packet << uint8(AUTH_OK);
		SendPacket(&packet);
	}
	else
	{
		WorldPacket packet(SMSG_AUTH_RESPONSE, 1 + 4);
		packet << uint8(AUTH_WAIT_QUEUE);
		packet << uint32(position);
		SendPacket(&packet);
	}
}
void WorldSession::ExecuteOpcode(OpcodeHandler const& opHandle, WorldPacket &packet)
{
	// need prevent do internal far teleports in handlers because some handlers do lot steps
	// or call code that can do far teleports in some conditions unexpectedly for generic way work code
	/*if (_player)
		_player->SetCanDelayTeleport(true);*/

	(this->*opHandle.handler)(packet);

	if (_player)
	{
		// can be not set in fact for login opcode, but this not create porblems.
		//_player->SetCanDelayTeleport(false);

		// we should execute delayed teleports only for alive(!) players
		// because we don't want player's ghost teleported from graveyard
		/*if (_player->IsHasDelayedTeleport())
			_player->TeleportTo(_player->m_teleport_dest, _player->m_teleport_options);*/
	}

	if (packet.rpos() < packet.wpos() && sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG))
		LogUnprocessedTail(packet);
}
/// %Log the player out
void WorldSession::LogoutPlayer(bool save)
{
	// if the player has just logged out, there is no need to do anything here
	if (m_playerRecentlyLogout)
		return;

	std::lock_guard<std::mutex> guard(m_logoutMutex);

	// finish pending transfers before starting the logout
	//while (_player && _player->IsBeingTeleportedFar())
		//HandleMoveWorldportAckOpcode();

	m_playerLogout = true;
	m_playerSave = save;

	if (_player)
	{
		sLog.outChar("Account: %d (IP: %s) Logout Character:[%s] (guid: %u)", GetAccountId(), GetRemoteAddress().c_str(), _player->GetName(), _player->GetGUIDLow());

		///- Reset the online field in the account table
		// no point resetting online in character table here as Player::SaveToDB() will set it to 1 since player has not been removed from world at this stage
		// No SQL injection as AccountID is uint32
		static SqlStatementID id;

		SqlStatement stmt = LoginDatabase.CreateStatement(id, "UPDATE account SET active_realm_id = ? WHERE id = ?");
		stmt.PExecute(uint32(0), GetAccountId());

		// some save parts only correctly work in case player present in map/player_lists (pets, etc)
		if (save)
			_player->SaveToDB();

		///- Remove the player from the world
		// the player may not be in the world when logging out
		// e.g if he got disconnected during a transfer to another map
		// calls to GetMap in this case may cause crashes
		if (_player->IsInWorld())
		{
			Map* _map = _player->GetMap();
			_map->Remove(_player, true);
		}
		else
		{
			_player->CleanupsBeforeDelete();
			Map::DeleteFromWorld(_player);
		}

		SetPlayer(nullptr);                                    // deleted in Remove/DeleteFromWorld call

															   ///- Send the 'logout complete' packet to the client
		WorldPacket data(SMSG_LOGOUT_COMPLETE, 0);
		SendPacket(&data);

		///- Since each account can only have one online character at any given time, ensure all characters for active account are marked as offline
		// No SQL injection as AccountId is uint32

		static SqlStatementID updChars;
		stmt = CharacterDatabase.CreateStatement(updChars, "UPDATE characters SET online = 0 WHERE account = ?");
		stmt.PExecute(GetAccountId());
		DEBUG_LOG("SESSION: Sent SMSG_LOGOUT_COMPLETE Message");
	}

	m_playerLogout = false;
	m_playerSave = false;
	m_playerRecentlyLogout = true;

	LogoutRequest(0);
}
/// Kick a player out of the World
void WorldSession::KickPlayer()
{
	if (!m_Socket->IsClosed())
		m_Socket->Close();
}