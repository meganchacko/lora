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

// Global schedule storage for network server access
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

    ////////////////////////////////////////
    // Task 2 — Burst Mode Detection
    ////////////////////////////////////////
    bool m_inBurstMac = false;

    std::map<std::pair<uint32_t, uint8_t>, uint32_t> m_successCount;
    std::map<std::pair<uint32_t, uint8_t>, uint32_t> m_collisionCount;

    void UpdateChannelStats(uint32_t freq, uint8_t sf, bool collision);
    void CheckBurstCondition(uint32_t freq, uint8_t sf);

    ////////////////////////////////////////
    // Task 3 — Virtual Channel Groups
    ////////////////////////////////////////
    // VC = (freq, SF) → set of device addresses
    std::map<std::pair<uint32_t, uint8_t>, std::set<LoraDeviceAddress>> m_vcGroups;

    void UpdateVcGroup(LoraDeviceAddress addr, uint32_t freq, uint8_t sf);

    ////////////////////////////////////////
    // Task 4 — Hash-Based Scheduling
    ////////////////////////////////////////
public:
    // For each device → assigned slot
    std::map<LoraDeviceAddress, uint32_t> m_slotAssignments;

    // For each VC → group size
    std::map<std::pair<uint32_t, uint8_t>, uint32_t> m_groupSizes;

private:
    // Task 5 — Collision Resolution
    // For each VC → per-device slot overrides assigned by gateway
    std::map<std::pair<uint32_t, uint8_t>, std::map<LoraDeviceAddress, uint32_t>> m_slotOverrides;

    // Recompute schedule for a given VC: detect collisions and reassign unused slots
    void RecomputeScheduleForVc(uint32_t freq, uint8_t sf);

    void SendScheduleToDevice(LoraDeviceAddress addr,
                              uint32_t freq,
                              uint8_t sf);

    ////////////////////////////////////////
    // Task 6 — Beaconing (Class B-like)
    ////////////////////////////////////////
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

#endif /* GATEWAY_LORAWAN_MAC_H */
