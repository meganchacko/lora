#include "burst-tag.h"
#include "ns3/log.h"

namespace ns3 {
namespace lorawan {

NS_LOG_COMPONENT_DEFINE("BurstTag");

TypeId
BurstTag::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::lorawan::BurstTag")
            .SetParent<Tag>()
            .AddConstructor<BurstTag>();
    return tid;
}

TypeId
BurstTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

BurstTag::BurstTag()
    : m_burst(false)
{
}

BurstTag::~BurstTag()
{
}

uint32_t
BurstTag::GetSerializedSize() const
{
    return 1;   // serialize as 1 byte
}

void
BurstTag::Serialize(TagBuffer buffer) const
{
    uint8_t val = m_burst ? 1 : 0;
    buffer.WriteU8(val);
}

void
BurstTag::Deserialize(TagBuffer buffer)
{
    uint8_t val = buffer.ReadU8();
    m_burst = (val == 1);
}

void
BurstTag::Print(std::ostream &os) const
{
    os << "Burst=" << (m_burst ? "true" : "false");
}

void
BurstTag::SetBurst(bool burst)
{
    m_burst = burst;
}

bool
BurstTag::GetBurst() const
{
    return m_burst;
}

} // namespace lorawan
} // namespace ns3
