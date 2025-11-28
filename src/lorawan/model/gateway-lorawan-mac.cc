/*
 * Copyright (c) 2017 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "gateway-lorawan-mac.h"

#include "lora-frame-header.h"
#include "lora-net-device.h"
#include "lorawan-mac-header.h"
#include "burst-tag.h"

#include "ns3/log.h"
#include "ns3/simulator.h"

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("GatewayLorawanMac");
NS_OBJECT_ENSURE_REGISTERED(GatewayLorawanMac);

TypeId
GatewayLorawanMac::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::GatewayLorawanMac")
            .SetParent<LorawanMac>()
            .AddConstructor<GatewayLorawanMac>()
            .SetGroupName("lorawan");
    return tid;
}

GatewayLorawanMac::GatewayLorawanMac()
{
    NS_LOG_FUNCTION(this);
}

GatewayLorawanMac::~GatewayLorawanMac()
{
    NS_LOG_FUNCTION(this);
}

bool
GatewayLorawanMac::IsTransmitting()
{
    return m_phy->IsTransmitting();
}

Time
GatewayLorawanMac::GetWaitTime(uint32_t frequencyHz)
{
    return m_channelHelper->GetWaitTime(frequencyHz);
}

void
GatewayLorawanMac::Send(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    LoraTag tag;
    packet->PeekPacketTag(tag);

    uint8_t dr = tag.GetDataRate();
    uint8_t sf = GetSfFromDataRate(dr);
    uint32_t freq = tag.GetFrequency();

    LoraTxParameters params;
    params.sf = sf;
    params.headerDisabled = false;
    params.codingRate = 1;
    params.bandwidthHz = GetBandwidthFromDataRate(dr);
    params.nPreamble = 8;
    params.crcEnabled = true;
    params.lowDataRateOptimizationEnabled = LoraPhy::GetTSym(params) > MilliSeconds(16);

    Time duration = LoraPhy::GetOnAirTime(packet, params);
    double txPower = m_channelHelper->GetTxPowerForChannel(freq);

    m_channelHelper->AddEvent(duration, freq);
    m_phy->Send(packet, params, freq, txPower);

    m_sentNewPacket(packet);
}
void
GatewayLorawanMac::Receive(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    // --------------------------------------------------
    // 1) Make sure m_device is valid and is LoraNetDevice
    // --------------------------------------------------
    if (m_device == nullptr)
    {
        Ptr<LoraNetDevice> dev = DynamicCast<LoraNetDevice>(GetDevice());
        if (dev)
        {
            m_device = dev;
        }
        else
        {
            NS_LOG_ERROR("GatewayLorawanMac: m_device is NULL and GetDevice() is not LoraNetDevice");
            return;
        }
    }

    Ptr<LoraNetDevice> loraDev = DynamicCast<LoraNetDevice>(m_device);
    if (!loraDev)
    {
        NS_LOG_ERROR("GatewayLorawanMac: m_device is not a LoraNetDevice");
        return;
    }

    // --------------------------------------------------
    // 2) Work on a copy of the packet
    // --------------------------------------------------
    Ptr<Packet> packetCopy = packet->Copy();

    // Read MAC header (from the front)
    LorawanMacHeader macHdr;
    packetCopy->PeekHeader(macHdr);

    // --------------------------------------------------
    // 3) Extract address correctly (strip MAC, then read FHDR)
    // --------------------------------------------------
    LoraDeviceAddress srcAddr;
    bool haveAddr = false;

    if (macHdr.GetMType() == LorawanMacHeader::UNCONFIRMED_DATA_UP ||
        macHdr.GetMType() == LorawanMacHeader::CONFIRMED_DATA_UP)
    {
        Ptr<Packet> tmp = packet->Copy();

        // First remove MAC header to move the iterator past it
        LorawanMacHeader tmpMac;
        tmp->RemoveHeader(tmpMac);

        // Now the next header is the LoraFrameHeader
        LoraFrameHeader fhdr;
        if (tmp->PeekHeader(fhdr))
        {
            srcAddr = fhdr.GetAddress();
            haveAddr = true;
        }
    }

    // --------------------------------------------------
    // 4) Extract frequency + SF from LoraTag
    // --------------------------------------------------
    uint32_t freq = 0;
    uint8_t sf = 0;

    LoraTag tag;
    if (packetCopy->PeekPacketTag(tag))
    {
        freq = tag.GetFrequency();
        uint8_t dr = tag.GetDataRate();
        sf = GetSfFromDataRate(dr);
    }

    // --------------------------------------------------
    // 5) Task 3: VC grouping (only for valid freq/SF + address)
    // --------------------------------------------------
    if (haveAddr && freq != 0 && sf != 0)
    {
        UpdateVcGroup(srcAddr, freq, sf);
    }

    // --------------------------------------------------
    // 6) Task 2: Burst detection from node (BurstTag)
    // --------------------------------------------------
    BurstTag burstTag;
    if (packetCopy->PeekPacketTag(burstTag) && burstTag.GetBurst())
    {
        if (!m_inBurstMac)
        {
            m_inBurstMac = true;
            NS_LOG_INFO("Gateway entering Burst-MAC mode due to node-side burst flag.");
        }
    }

    // --------------------------------------------------
    // 7) Task 2: Collision stats (this is a successful reception)
    // --------------------------------------------------
    if (freq != 0 && sf != 0)
    {
        UpdateChannelStats(freq, sf, false /* not a collision */);
    }

    // --------------------------------------------------
    // 8) Forward only uplink packets to the net device
    // --------------------------------------------------
    if (macHdr.IsUplink())
    {
        loraDev->Receive(packetCopy);

        NS_LOG_DEBUG("Received packet: " << packet);

        m_receivedPacket(packet);
    }
    else
    {
        NS_LOG_DEBUG("Not forwarding downlink message to NetDevice");
    }
}

void
GatewayLorawanMac::FailedReception(Ptr<const Packet> packet)
{
    LoraTag tag;
    if (packet->PeekPacketTag(tag))
    {
        uint32_t freq = tag.GetFrequency();
        uint8_t dr = tag.GetDataRate();
        uint8_t sf = GetSfFromDataRate(dr);

        UpdateChannelStats(freq, sf, true);
    }
}

void
GatewayLorawanMac::TxFinished(Ptr<const Packet> packet)
{
}

void
GatewayLorawanMac::UpdateVcGroup(LoraDeviceAddress addr, uint32_t freq, uint8_t sf)
{
    auto key = std::make_pair(freq, sf);
    m_vcGroups[key].insert(addr);

    NS_LOG_INFO("VC Update: Device " << addr.Get() << " on (freq="
                                     << freq << " Hz, SF" << unsigned(sf) << ")");
}

void
GatewayLorawanMac::UpdateChannelStats(uint32_t freq, uint8_t sf, bool collision)
{
    auto key = std::make_pair(freq, sf);

    if (collision)
        m_collisionCount[key]++;
    else
        m_successCount[key]++;

    CheckBurstCondition(freq, sf);
}

void
GatewayLorawanMac::CheckBurstCondition(uint32_t freq, uint8_t sf)
{
    auto key = std::make_pair(freq, sf);

    uint32_t succ = m_successCount[key];
    uint32_t coll = m_collisionCount[key];

    if (coll > succ && !m_inBurstMac)
    {
        m_inBurstMac = true;
        NS_LOG_INFO("Gateway entering Burst-MAC due to high collision rate "
                    << "(freq=" << freq << ", SF=" << unsigned(sf) << ")");
    }
}

} // namespace lorawan
} // namespace ns3
