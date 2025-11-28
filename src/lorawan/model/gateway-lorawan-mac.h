/*
 * Copyright (c) 2017 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 */

#ifndef GATEWAY_LORAWAN_MAC_H
#define GATEWAY_LORAWAN_MAC_H

#include "lora-tag.h"
#include "lorawan-mac.h"
#include "lora-frame-header.h"
#include "lora-device-address.h"

#include <map>
#include <set>

namespace ns3
{
namespace lorawan
{

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

    // ---------------------
    // Task 2 — Burst MAC
    // ---------------------
    bool m_inBurstMac = false;  

    // Track collisions and successes per (freq, SF)
    std::map<std::pair<uint32_t, uint8_t>, uint32_t> m_successCount;
    std::map<std::pair<uint32_t, uint8_t>, uint32_t> m_collisionCount;

    void UpdateChannelStats(uint32_t freq, uint8_t sf, bool collision);

    // Trigger Burst-MAC when collision threshold exceeded
    void CheckBurstCondition(uint32_t freq, uint8_t sf);

    // ---------------------
    // Task 3 — Virtual Channels
    // ---------------------
    // VC = (frequency, SF) → set of device addresses
    std::map<std::pair<uint32_t, uint8_t>, std::set<LoraDeviceAddress>> m_vcGroups;

    void UpdateVcGroup(LoraDeviceAddress addr, uint32_t freq, uint8_t sf);

protected:
};

} // namespace lorawan
} // namespace ns3

#endif /* GATEWAY_LORAWAN_MAC_H */
