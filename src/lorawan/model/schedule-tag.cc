#include "schedule-tag.h"
#include "ns3/log.h"

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("ScheduleTag");

ScheduleTag::ScheduleTag()
    : m_slot(0),
      m_groupSize(1),
      m_sf(7)
{
}

ScheduleTag::~ScheduleTag()
{
}

TypeId
ScheduleTag::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::lorawan::ScheduleTag")
            .SetParent<Tag>()
            .AddConstructor<ScheduleTag>();
    return tid;
}

TypeId
ScheduleTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
ScheduleTag::GetSerializedSize() const
{
    // slot (4 bytes) + group (4 bytes) + sf (1 byte)
    return 4 + 4 + 1;
}

void
ScheduleTag::Serialize(TagBuffer buf) const
{
    buf.WriteU32(m_slot);
    buf.WriteU32(m_groupSize);
    buf.WriteU8(m_sf);
}

void
ScheduleTag::Deserialize(TagBuffer buf)
{
    m_slot = buf.ReadU32();
    m_groupSize = buf.ReadU32();
    m_sf = buf.ReadU8();
}

void
ScheduleTag::Print(std::ostream &os) const
{
    os << "slot=" << m_slot
       << ", groupSize=" << m_groupSize
       << ", sf=" << unsigned(m_sf);
}

void
ScheduleTag::Set(uint32_t slot, uint32_t groupSize, uint8_t sf)
{
    m_slot = slot;
    m_groupSize = groupSize;
    m_sf = sf;
}

uint32_t
ScheduleTag::GetSlot() const
{
    return m_slot;
}

uint32_t
ScheduleTag::GetGroupSize() const
{
    return m_groupSize;
}

uint8_t
ScheduleTag::GetSf() const
{
    return m_sf;
}

} // namespace lorawan
} // namespace ns3
