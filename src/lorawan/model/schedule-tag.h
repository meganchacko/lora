#ifndef SCHEDULE_TAG_H
#define SCHEDULE_TAG_H

#include "ns3/tag.h"
#include "ns3/uinteger.h"

namespace ns3
{
namespace lorawan
{

class ScheduleTag : public Tag
{
  public:
    ScheduleTag();
    ScheduleTag(uint8_t vc, uint16_t slot, bool reassigned);

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;

    // Required Tag methods
    uint32_t GetSerializedSize() const override;
    void Serialize(TagBuffer buffer) const override;
    void Deserialize(TagBuffer buffer) override;
    void Print(std::ostream& os) const override;

    // Getters
    uint8_t GetVc() const;
    uint16_t GetSlot() const;
    bool GetReassigned() const;

  private:
    uint8_t m_vc;
    uint16_t m_slot;
    bool m_reassigned;
};

} // namespace lorawan
} // namespace ns3

#endif // SCHEDULE_TAG_H
