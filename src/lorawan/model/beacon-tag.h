#ifndef BEACON_TAG_H
#define BEACON_TAG_H

#include "ns3/tag.h"

namespace ns3 {
namespace lorawan {

// Simple tag to mark gateway beacons used for burst (Class B-like) sync
class BeaconTag : public Tag {
public:
    BeaconTag();
    ~BeaconTag() override;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;

    uint32_t GetSerializedSize() const override;
    void Serialize(TagBuffer buf) const override;
    void Deserialize(TagBuffer buf) override;
    void Print(std::ostream &os) const override;

    void SetEpoch(uint32_t epochMs);
    uint32_t GetEpoch() const;

private:
    uint32_t m_epochMs {0};
};

} // namespace lorawan
} // namespace ns3

#endif // BEACON_TAG_H