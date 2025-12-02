#include "schedule-tag.h"

#include "ns3/log.h"
#include "ns3/type-id.h"

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("ScheduleTag");

ScheduleTag::ScheduleTag()
    : m_vc(0),
      m_slot(0),
      m_reassigned(false)
{
}

ScheduleTag::ScheduleTag(uint8_t vc, uint16_t slot, bool reassigned)
    : m_vc(vc),
      m_slot(slot),
      m_reassigned(reassigned)
{
}

TypeId
ScheduleTag::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::lorawan::ScheduleTag").SetParent<Tag>().AddConstructor<ScheduleTag>();
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
    return sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint8_t);
}

void
ScheduleTag::Serialize(TagBuffer buffer) const
{
    buffer.WriteU8(m_vc);
    buffer.WriteU16(m_slot);
    buffer.WriteU8(m_reassigned ? 1 : 0);
}

void
ScheduleTag::Deserialize(TagBuffer buffer)
{
    m_vc = buffer.ReadU8();
    m_slot = buffer.ReadU16();
    m_reassigned = (buffer.ReadU8() == 1);
}

void
ScheduleTag::Print(std::ostream& os) const
{
    os << "ScheduleTag { VC=" << unsigned(m_vc) << ", slot=" << m_slot
       << ", reassigned=" << std::boolalpha << m_reassigned << " }";
}

uint8_t
ScheduleTag::GetVc() const
{
    return m_vc;
}

uint16_t
ScheduleTag::GetSlot() const
{
    return m_slot;
}

bool
ScheduleTag::GetReassigned() const
{
    return m_reassigned;
}

} // namespace lorawan
} // namespace ns3
