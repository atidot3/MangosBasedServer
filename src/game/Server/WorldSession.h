#ifndef __WORLDSESSION_H
#define __WORLDSESSION_H

#include <Common.h>
#include "SharedDefine.h"
#include "../Object/ObjectGuid.h"
#include "WorldSocket.h"

#include <deque>
#include <mutex>
#include <memory>

class ObjectGuid;
class Object;
class Player;
class WorldPacket;
class QueryResult;
class LoginQueryHolder;
class CharacterHandler;
class WorldSession;

struct OpcodeHandler;

enum PartyOperation
{
	PARTY_OP_INVITE = 0,
	PARTY_OP_LEAVE = 2,
};

enum PartyResult
{
	ERR_PARTY_RESULT_OK = 0,
	ERR_BAD_PLAYER_NAME_S = 1,
	ERR_TARGET_NOT_IN_GROUP_S = 2,
	ERR_GROUP_FULL = 3,
	ERR_ALREADY_IN_GROUP_S = 4,
	ERR_NOT_IN_GROUP = 5,
	ERR_NOT_LEADER = 6,
	ERR_PLAYER_WRONG_FACTION = 7,
	ERR_IGNORING_YOU_S = 8
};

enum TutorialDataState
{
	TUTORIALDATA_UNCHANGED = 0,
	TUTORIALDATA_CHANGED = 1,
	TUTORIALDATA_NEW = 2
};

// class to deal with packet processing
// allows to determine if next packet is safe to be processed
class PacketFilter
{
public:
	explicit PacketFilter(WorldSession* pSession) : m_pSession(pSession) {}
	virtual ~PacketFilter() {}

	virtual bool Process(WorldPacket* /*packet*/) { return true; }
	virtual bool ProcessLogout() const { return true; }

protected:
	WorldSession* const m_pSession;
};
// process only thread-safe packets in Map::Update()
class MapSessionFilter : public PacketFilter
{
public:
	explicit MapSessionFilter(WorldSession* pSession) : PacketFilter(pSession) {}
	~MapSessionFilter() {}

	virtual bool Process(WorldPacket* packet) override;
	// in Map::Update() we do not process player logout!
	virtual bool ProcessLogout() const override { return false; }
};

// class used to filer only thread-unsafe packets from queue
// in order to update only be used in World::UpdateSessions()
class WorldSessionFilter : public PacketFilter
{
public:
	explicit WorldSessionFilter(WorldSession* pSession) : PacketFilter(pSession) {}
	~WorldSessionFilter() {}

	virtual bool Process(WorldPacket* packet) override;
};

/// Player session in the World
class WorldSession
{
	friend class CharacterHandler;

public:
	WorldSession(uint32 id, WorldSocket* sock, AccountTypes sec, time_t mute_time, LocaleConstant locale);
	~WorldSession();

	bool PlayerLoading() const { return m_playerLoading; }
	bool PlayerLogout() const { return m_playerLogout; }
	bool PlayerLogoutWithSave() const { return m_playerLogout && m_playerSave; }

	void SizeError(WorldPacket const& packet, uint32 size) const;

	void SendPacket(WorldPacket const* packet, bool immediate = false);
	void SendQueryTimeResponse();

	AccountTypes GetSecurity() const { return _security; }
	uint32 GetAccountId() const { return _accountId; }
	Player* GetPlayer() const { return _player; }
	char const* GetPlayerName() const;
	void SetSecurity(AccountTypes security) { _security = security; }
	const std::string &GetRemoteAddress() const { return m_Socket->GetRemoteAddress(); }

	/// Session in auth.queue currently
	void SetInQueue(bool state) { m_inQueue = state; }

	/// Is the user engaged in a log out process?
	bool isLogingOut() const { return _logoutTime || m_playerLogout; }

	/// Engage the logout process for the user
	void LogoutRequest(time_t requestTime)
	{
		_logoutTime = requestTime;
	}

	/// Is logout cooldown expired?
	bool ShouldLogOut(time_t currTime) const
	{
		return (_logoutTime > 0 && currTime >= _logoutTime + 20);
	}

	void QueuePacket(std::unique_ptr<WorldPacket> new_packet);

	bool Update(PacketFilter& updater);

	/// Handle the authentication waiting queue (to be completed)
	void SendAuthWaitQue(uint32 position);

	void SetPlayer(Player* plr) { _player = plr; }
	
	void LogoutPlayer(bool save);
	void KickPlayer();
	
	// Account mute time
	time_t m_muteTime;

	uint32 GetLatency() const { return m_latency; }
	void SetLatency(uint32 latency) { m_latency = latency; }
	void ResetClientTimeDelay() { m_clientTimeDelay = 0; }

	// opcodes handlers
	void Handle_NULL(WorldPacket& recvPacket);          // not used
	void Handle_EarlyProccess(WorldPacket& recvPacket); // just mark packets processed in WorldSocket::OnRead
	void Handle_ServerSide(WorldPacket& recvPacket);    // sever side only, can't be accepted from client
	void Handle_Deprecated(WorldPacket& recvPacket);    // never used anymore by client

	void HandleCharEnumOpcode(WorldPacket& recvPacket);
	void HandleCharDeleteOpcode(WorldPacket& recvPacket);
	void HandleCharCreateOpcode(WorldPacket& recvPacket);
	void HandlePlayerLoginOpcode(WorldPacket& recvPacket);
	void HandleCharEnum(QueryResult* result);
	void HandlePlayerLogin(LoginQueryHolder* holder);
	void HandlePlayerEnterWorldfinished(WorldPacket& recv_data);

	void HandleLogoutRequestOpcode(WorldPacket& recvPacket);
	void HandlePlayerLogoutOpcode(WorldPacket& recvPacket);
	void HandleLogoutCancelOpcode(WorldPacket& recvPacket);

	// movement
	void HandleMovementOpcodes(WorldPacket& recvPacket);
	void HandleMoveOpcodes(WorldPacket& packet);
	void HandleMoveRelocate(WorldPacket& packet);
private:
	void ExecuteOpcode(OpcodeHandler const& opHandle, WorldPacket& packet);

	// logging helper
	void LogUnexpectedOpcode(WorldPacket const& packet, const char* reason);
	void LogUnprocessedTail(WorldPacket const& packet);

	std::mutex m_logoutMutex;                           // this mutex is necessary to avoid two simultaneous logouts due to a valid logout request and socket error
	Player * _player; 
	WorldSocket * const m_Socket;                       // socket pointer is owned by the network thread which created 

	AccountTypes _security;
	uint32 _accountId;

	time_t _logoutTime;
	bool m_inQueue;                                     // session wait in auth.queue
	bool m_playerLoading;                               // code processed in LoginPlayer

														// True when the player is in the process of logging out (WorldSession::LogoutPlayer is currently executing)
	bool m_playerLogout;
	bool m_playerRecentlyLogout;
	bool m_playerSave;                                  // code processed in LogoutPlayer with save request
	uint32 m_latency;
	uint32 m_clientTimeDelay;
	uint32 m_Tutorials[8];
	TutorialDataState m_tutorialState;

	std::mutex m_recvQueueLock;
	std::deque<std::unique_ptr<WorldPacket>> m_recvQueue;
};
#endif
/// @}
