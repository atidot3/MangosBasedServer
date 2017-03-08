/*
* This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/** \file
\ingroup u2w
*/

#include "Opcodes.h"
#include <config\Singleton.h>

INSTANTIATE_SINGLETON_1(Opcodes);

OpcodeHandler const Opcodes::emptyHandler =
{
	"<none>",
	STATUS_UNHANDLED,
	PROCESS_INPLACE,
	&WorldSession::Handle_NULL
};


Opcodes::Opcodes()
{
	/// Build Opcodes map
	BuildOpcodeList();
}

Opcodes::~Opcodes()
{
	/// Clear Opcodes
	mOpcodeMap.clear();
}


void Opcodes::BuildOpcodeList()
{
	StoreOpcode(MSG_NULL_ACTION, "MSG_NULL_ACTION", STATUS_NEVER, PROCESS_INPLACE, &WorldSession::Handle_NULL);
	StoreOpcode(SMSG_AUTH_CHALLENGE, "SMSG_AUTH_CHALLENGE", STATUS_NEVER, PROCESS_INPLACE, &WorldSession::Handle_ServerSide);
	StoreOpcode(CMSG_AUTH_SESSION, "CMSG_AUTH_SESSION", STATUS_NEVER, PROCESS_THREADSAFE, &WorldSession::Handle_EarlyProccess);
	StoreOpcode(CMSG_CHAR_CREATE, "CMSG_CHAR_CREATE", STATUS_AUTHED, PROCESS_THREADUNSAFE, &WorldSession::HandleCharCreateOpcode);
	StoreOpcode(CMSG_CHAR_ENUM, "CMSG_CHAR_ENUM", STATUS_AUTHED, PROCESS_THREADUNSAFE, &WorldSession::HandleCharEnumOpcode);
	StoreOpcode(CMSG_CHAR_DELETE, "CMSG_CHAR_DELETE", STATUS_AUTHED, PROCESS_THREADUNSAFE, &WorldSession::HandleCharDeleteOpcode);
	StoreOpcode(CMSG_PLAYER_LOGIN, "CMSG_PLAYER_LOGIN", STATUS_AUTHED, PROCESS_INPLACE, &WorldSession::HandlePlayerLoginOpcode);
	StoreOpcode(CMSG_PLAYER_LOGOUT, "CMSG_PLAYER_LOGOUT", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandlePlayerLogoutOpcode);
	StoreOpcode(CMSG_LOGOUT_REQUEST, "CMSG_LOGOUT_REQUEST", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleLogoutRequestOpcode);
	StoreOpcode(CMSG_LOGOUT_CANCEL, "CMSG_LOGOUT_CANCEL", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleLogoutCancelOpcode);
	StoreOpcode(CMSG_PING, "CMSG_PING", STATUS_NEVER, PROCESS_INPLACE, &WorldSession::Handle_EarlyProccess);
	StoreOpcode(CMSG_KEEP_ALIVE, "CMSG_KEEP_ALIVE", STATUS_NEVER, PROCESS_INPLACE, &WorldSession::Handle_EarlyProccess);
	StoreOpcode(CMSG_ENTER_WORLD_FINISHED, "CMSG_ENTER_WORLD_FINISHED", STATUS_AUTHED, PROCESS_THREADSAFE, &WorldSession::HandlePlayerEnterWorldfinished);
	
	StoreOpcode(MSG_MOVEMENT, "MSG_MOVEMENT", STATUS_LOGGEDIN, PROCESS_THREADSAFE, &WorldSession::HandleMovementOpcodes);
	StoreOpcode(MSG_RECLOCATE, "MSG_RECLOCATE", STATUS_LOGGEDIN, PROCESS_THREADSAFE, &WorldSession::HandleMovementOpcodes);
	StoreOpcode(MSG_MOVE_JUMP, "MSG_MOVE_JUMP", STATUS_LOGGEDIN, PROCESS_THREADSAFE, &WorldSession::HandleMovementOpcodes);

	return; 
}
