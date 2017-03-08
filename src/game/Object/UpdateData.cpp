#include "Common.h"
#include "UpdateData.h"
#include "ByteBuffer.h"
#include "WorldPacket.h"
#include "Log.h"
#include "Opcodes.h"
#include "World.h"
#include "ObjectGuid.h"

UpdateData::UpdateData() : m_blockCount(0)
{
}

void UpdateData::AddOutOfRangeGUID(GuidSet& guids)
{
	m_outOfRangeGUIDs.insert(guids.begin(), guids.end());
}

void UpdateData::AddOutOfRangeGUID(ObjectGuid const& guid)
{
	m_outOfRangeGUIDs.insert(guid);
}

void UpdateData::AddUpdateBlock(const ByteBuffer& block)
{
	m_data.append(block);
	++m_blockCount;
}

bool UpdateData::BuildPacket(WorldPacket* packet, bool hasTransport)
{
	ORIGIN_ASSERT(packet->empty());                         // shouldn't happen

	ByteBuffer buf(4 + 1 + (m_outOfRangeGUIDs.empty() ? 0 : 1 + 4 + 9 * m_outOfRangeGUIDs.size()) + m_data.wpos());

	buf << (uint32)(!m_outOfRangeGUIDs.empty() ? m_blockCount + 1 : m_blockCount);
	buf << (uint8)(hasTransport ? 1 : 0);

	if (!m_outOfRangeGUIDs.empty())
	{
		buf << (uint8)UPDATETYPE_OUT_OF_RANGE_OBJECTS;
		buf << (uint32)m_outOfRangeGUIDs.size();

		for (GuidSet::const_iterator i = m_outOfRangeGUIDs.begin(); i != m_outOfRangeGUIDs.end(); ++i)
			buf << i->WriteAsPacked();
	}

	buf.append(m_data);

	packet->append(buf);
	packet->SetOpcode(SMSG_UPDATE_OBJECT);

	return true;
}

void UpdateData::Clear()
{
	m_data.clear();
	m_outOfRangeGUIDs.clear();
	m_blockCount = 0;
}
