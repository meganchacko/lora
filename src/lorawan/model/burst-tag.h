#ifndef BURST_TAG_H
#define BURST_TAG_H

#include "ns3/tag.h"
#include "ns3/nstime.h"
#include "ns3/uinteger.h"

namespace ns3 {
namespace lorawan {

class BurstTag : public Tag
{
public:
    BurstTag();
    virtual ~BurstTag();

    static TypeId GetTypeId();
    virtual TypeId GetInstanceTypeId() const override;

    virtual uint32_t GetSerializedSize() const override;
    virtual void Serialize(TagBuffer buffer) const override;
    virtual void Deserialize(TagBuffer buffer) override;
    virtual void Print(std::ostream &os) const override;

    void SetBurst(bool burst);
    bool GetBurst() const;

private:
    bool m_burst;
};

} // namespace lorawan
} // namespace ns3

#endif
