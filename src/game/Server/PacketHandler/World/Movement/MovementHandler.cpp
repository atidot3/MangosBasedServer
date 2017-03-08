#include <Common.h>
#include <Database/DatabaseEnv.h>
#include <WorldPacket.h>
#include "../../../SharedDefine.h"
#include "../../../WorldSession.h"
#include "../../../Opcodes.h"
#include <Log.h>
#include "../../../../World/World.h"
#include <Database/DatabaseImpl.h>
#include "../../../../Object/Player.h"
#include "ObjectMgr.h"
#include <Util\Util.h>

void WorldSession::HandleMoveOpcodes(WorldPacket& packet)
{
	uint16 opcode = packet.GetOpcode();
	float x, y, z, o;

	packet >> x;
	packet >> y;
	packet >> z;
	packet >> o;

	WorldPacket data(opcode, 4 + 4 + 4 + 4 + 4);
	data << _player->GetGUIDLow();
	data << x;
	data << y;
	data << z;
	data << o;

	_player->SendToOther(data, false);
	_player->Relocate(x, y, z, o);
}
void WorldSession::HandleMoveRelocate(WorldPacket& packet)
{
	uint16 opcode = packet.GetOpcode();
	float x, y, z, o;
	packet >> x;
	packet >> y;
	packet >> z;
	packet >> o;
	
	WorldPacket data(opcode, 4 + 4 + 4 + 4 + 4);
	data << _player->GetGUIDLow();
	data << x;
	data << y;
	data << z;
	data << o;

	_player->SendToOther(data, false);
	_player->Relocate(x, y, z, o);
}
void WorldSession::HandleMovementOpcodes(WorldPacket& recvPacket)
{
	uint16 opcode = recvPacket.GetOpcode();
	

	if (!sLog.HasLogFilter(LOG_FILTER_PLAYER_MOVES))
	{
		DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(opcode), opcode, opcode);
		recvPacket.hexlike();
	}
	if (opcode == MSG_MOVEMENT)
	{
		HandleMoveOpcodes(recvPacket);
	}
	else if (opcode == MSG_RECLOCATE)
		HandleMoveRelocate(recvPacket);
	else if (opcode == MSG_MOVE_JUMP)
	{
		WorldPacket data(opcode, 4);
		data << _player->GetGUIDLow();
	}
}