#ifndef ORIGINSERVER_WORLDPACKET_H
#define ORIGINSERVER_WORLDPACKET_H

#include "../Common.h"
#include "ByteBuffer.h"
#include "../../game/Server/Opcodes.h"

// Note: m_opcode and size stored in platfom dependent format
// ignore endianess until send, and converted at receive
class WorldPacket : public ByteBuffer
{
public:
	// just container for later use
	WorldPacket() : ByteBuffer(0), m_opcode(MSG_NULL_ACTION)
	{
	}
	explicit WorldPacket(uint16 opcode, size_t res = 200) : ByteBuffer(res), m_opcode(opcode) { }
	// copy constructor
	WorldPacket(const WorldPacket& packet) : ByteBuffer(packet), m_opcode(packet.m_opcode)
	{
	}

	void Initialize(uint16 opcode, size_t newres = 200)
	{
		clear();
		_storage.reserve(newres);
		m_opcode = opcode;
	}

	uint16 GetOpcode() const { return m_opcode; }
	void SetOpcode(uint16 opcode) { m_opcode = opcode; }
	inline const char* GetOpcodeName() const { return LookupOpcodeName(m_opcode); }

protected:
	uint16 m_opcode;
};
#endif