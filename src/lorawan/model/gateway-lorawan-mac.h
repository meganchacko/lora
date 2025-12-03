#ifndef GATEWAY_LORAWAN_MAC_H
#define GATEWAY_LORAWAN_MAC_H

#include "lora-tag.h"
#include "lorawan-mac.h"
#include "lora-frame-header.h"
#include "lora-device-address.h"
#include "schedule-tag.h"
#include "beacon-tag.h"

#include <map>
#include <set>
#include <vector>

namespace ns3
{
namespace lorawan
{

extern std::map<LoraDeviceAddress, std::tuple<uint32_t, uint32_t, uint8_t>> g_pendingSchedules;

class GatewayLorawanMac : public LorawanMac
{
public:
    static TypeId GetTypeId();

    GatewayLorawanMac();
    ~GatewayLorawanMac() override;

    void Send(Ptr<Packet> packet) override;
    void Receive(Ptr<const Packet> packet) override;
    void FailedReception(Ptr<const Packet> packet) override;
    void TxFinished(Ptr<const Packet> packet) override;

    bool IsTransmitting();
    Time GetWaitTime(uint32_t frequencyHz);

private:

  
    bool m_inBurstMac = false;

    std::map<std::pair<uint32_t, uint8_t>, uint32_t> m_successCount;
    std::map<std::pair<uint32_t, uint8_t>, uint32_t> m_collisionCount;

    void UpdateChannelStats(uint32_t freq, uint8_t sf, bool collision);
    void CheckBurstCondition(uint32_t freq, uint8_t sf);

   
    std::map<std::pair<uint32_t, uint8_t>, std::set<LoraDeviceAddress>> m_vcGroups;

    void UpdateVcGroup(LoraDeviceAddress addr, uint32_t freq, uint8_t sf);


public:
    std::map<LoraDeviceAddress, uint32_t> m_slotAssignments;

    std::map<std::pair<uint32_t, uint8_t>, uint32_t> m_groupSizes;

private:
   
    std::map<std::pair<uint32_t, uint8_t>, std::map<LoraDeviceAddress, uint32_t>> m_slotOverrides;

    void RecomputeScheduleForVc(uint32_t freq, uint8_t sf);

    void SendScheduleToDevice(LoraDeviceAddress addr,
                              uint32_t freq,
                              uint8_t sf);

public:
    void StartBeacons(Time period);
private:
    void SendBeacon();
    Time m_beaconPeriod = Seconds(0);
    EventId m_beaconEvent;    

protected:
};

} // namespace lorawan
} // namespace ns3

#endif 
