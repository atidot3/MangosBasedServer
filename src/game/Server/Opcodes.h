#ifndef _OPCODES_H
#define _OPCODES_H

#include <Common.h>

// Note: this include need for be sure have full definition of class WorldSession
//       if this class definition not complite then VS for x64 release use different size for
//       struct OpcodeHandler in this header and Opcode.cpp and get totally wrong data from
//       table opcodeTable in source when Opcode.h included but WorldSession.h not included
#include "WorldSession.h"
#include <config\Singleton.h>

/// List of Opcodes
enum OpcodesList
{
	MSG_NULL_ACTION = 0x000,
	SMSG_AUTH_CHALLENGE = 0x001,	// ASK CLIENT TO LOGIN
	CMSG_AUTH_SESSION = 0x002,		// CLIENT LOGIN PACKET
	SMSG_AUTH_RESPONSE = 0x003,		// SEND RESULT OF LOGIN 
	CMSG_CHAR_CREATE = 0x004,
	SMSG_CHAR_CREATE = 0x005,
	CMSG_CHAR_ENUM = 0x006,
	SMSG_CHAR_ENUM = 0x007,
	CMSG_CHAR_DELETE = 0x008,
	SMSG_CHAR_DELETE = 0x009,
	CMSG_PLAYER_LOGIN = 0x00a,
	CMSG_PLAYER_LOGOUT = 0x00b,
	SMSG_LOGOUT_COMPLETE = 0x00c,
	CMSG_LOGOUT_REQUEST = 0x00d,
	CMSG_LOGOUT_CANCEL = 0x00e,
	CMSG_PING = 0x00f,
	SMSG_PONG = 0x010,
	CMSG_KEEP_ALIVE = 0x011,
	SMSG_LOGIN_VERIFY_WORLD = 0x012,
	SMSG_LOGIN_FINISHED = 0x013,
	CMSG_ENTER_WORLD_FINISHED = 0x014,
	SMSG_CREATE_OBJECT = 0x015,
	SMSG_UPDATE_OBJECT = 0x016,
	SMSG_DESTROY_OBJECT = 0x017,

	MSG_MOVEMENT = 0x018,
	MSG_RECLOCATE = 0x019,
	MSG_MOVE_JUMP = 0x01A,

	SMG_END = 0x01B
};

// Don't forget to change this value and add opcode name to Opcodes.cpp when you add new opcode!
#define NUM_MSG_TYPES SMG_END

/// Player state
enum SessionStatus
{
	STATUS_AUTHED = 0,                                      ///< Player authenticated (_player==nullptr, m_playerRecentlyLogout = false or will be reset before handler call)
	STATUS_LOGGEDIN,                                        ///< Player in game (_player!=nullptr, inWorld())
	STATUS_TRANSFER,                                        ///< Player transferring to another map (_player!=nullptr, !inWorld())
	STATUS_LOGGEDIN_OR_RECENTLY_LOGGEDOUT,                  ///< _player!= nullptr or _player==nullptr && m_playerRecentlyLogout)
	STATUS_NEVER,                                           ///< Opcode not accepted from client (deprecated or server side only)
	STATUS_UNHANDLED                                        ///< We don' handle this opcode yet
};

enum PacketProcessing
{
	PROCESS_INPLACE = 0,                                    // process packet whenever we receive it - mostly for non-handled or non-implemented packets
	PROCESS_THREADUNSAFE,                                   // packet is not thread-safe - process it in World::UpdateSessions()
	PROCESS_THREADSAFE                                      // packet is thread-safe - process it in Map::Update()
};

class WorldPacket;

struct OpcodeHandler
{
	char const* name;
	SessionStatus status;
	PacketProcessing packetProcessing;
	void (WorldSession::*handler)(WorldPacket& recvPacket);
};

typedef std::map< uint16, OpcodeHandler> OpcodeMap;

class Opcodes
{
public:
	Opcodes();
	~Opcodes();
public:
	void BuildOpcodeList();
	void StoreOpcode(uint16 Opcode, char const* name, SessionStatus status, PacketProcessing process, void (WorldSession::*handler)(WorldPacket& recvPacket))
	{
		OpcodeHandler& ref = mOpcodeMap[Opcode];
		ref.name = name;
		ref.status = status;
		ref.packetProcessing = process;
		ref.handler = handler;
	}

	/// Lookup opcode
	inline OpcodeHandler const* LookupOpcode(uint16 id) const
	{
		OpcodeMap::const_iterator itr = mOpcodeMap.find(id);
		if (itr != mOpcodeMap.end())
			return &itr->second;
		return nullptr;
	}

	/// compatible with other mangos branches access

	inline OpcodeHandler const& operator[](uint16 id) const
	{
		OpcodeMap::const_iterator itr = mOpcodeMap.find(id);
		if (itr != mOpcodeMap.end())
			return itr->second;
		return emptyHandler;
	}

	static OpcodeHandler const emptyHandler;

	OpcodeMap mOpcodeMap;
};

#define opcodeTable Origin::Singleton<Opcodes>::Instance()

/// Lookup opcode name for human understandable logging
inline char const* LookupOpcodeName(uint16 id)
{
	if (OpcodeHandler const* op = opcodeTable.LookupOpcode(id))
		return op->name;
	return "Received unknown opcode, it's more than max!";
}
#endif
/// @}
