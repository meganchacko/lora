#ifndef SCHEDULE_TAG_H
#define SCHEDULE_TAG_H

#include "ns3/tag.h"
#include "ns3/uinteger.h"
#include "ns3/log.h"

namespace ns3
{
namespace lorawan
{

class ScheduleTag : public Tag
{
public:
    ScheduleTag();
    virtual ~ScheduleTag();

    static TypeId GetTypeId();
    virtual TypeId GetInstanceTypeId() const override;

    virtual uint32_t GetSerializedSize() const override;
    virtual void Serialize(TagBuffer buf) const override;
    virtual void Deserialize(TagBuffer buf) override;
    virtual void Print(std::ostream &os) const override;

    void Set(uint32_t slot, uint32_t groupSize, uint8_t sf);

    uint32_t GetSlot() const;
    uint32_t GetGroupSize() const;
    uint8_t GetSf() const;

private:
    uint32_t m_slot;
    uint32_t m_groupSize;
    uint8_t  m_sf;
};

} // namespace lorawan
} // namespace ns3

#endif // SCHEDULE_TAG_H
