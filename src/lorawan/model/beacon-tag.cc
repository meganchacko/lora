#include "beacon-tag.h"
#include "ns3/log.h"

namespace ns3 {
namespace lorawan {

NS_LOG_COMPONENT_DEFINE("BeaconTag");

BeaconTag::BeaconTag() = default;
BeaconTag::~BeaconTag() = default;

TypeId BeaconTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::lorawan::BeaconTag")
                            .SetParent<Tag>()
                            .AddConstructor<BeaconTag>();
    return tid;
}

TypeId BeaconTag::GetInstanceTypeId() const { return GetTypeId(); }

uint32_t BeaconTag::GetSerializedSize() const { return 4; }

void BeaconTag::Serialize(TagBuffer buf) const { buf.WriteU32(m_epochMs); }

void BeaconTag::Deserialize(TagBuffer buf) { m_epochMs = buf.ReadU32(); }

void BeaconTag::Print(std::ostream &os) const { os << "epochMs=" << m_epochMs; }

void BeaconTag::SetEpoch(uint32_t epochMs) { m_epochMs = epochMs; }
uint32_t BeaconTag::GetEpoch() const { return m_epochMs; }

} // namespace lorawan
} // namespace ns3
