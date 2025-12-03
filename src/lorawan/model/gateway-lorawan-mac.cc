#include "gateway-lorawan-mac.h"

#include "lora-frame-header.h"
#include "lora-net-device.h"
#include "lorawan-mac-header.h"
#include "burst-tag.h"
#include "schedule-tag.h"
#include "beacon-tag.h"

#include "ns3/log.h"
#include "ns3/simulator.h"

namespace ns3
{
namespace lorawan
{

// Global schedule storage definition
std::map<LoraDeviceAddress, std::tuple<uint32_t, uint32_t, uint8_t>> g_pendingSchedules;

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
    params.lowDataRateOptimizationEnabled =
        LoraPhy::GetTSym(params) > MilliSeconds(16);

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

    // Validate NetDevice
    if (m_device == nullptr)
    {
        Ptr<LoraNetDevice> dev = DynamicCast<LoraNetDevice>(GetDevice());
        if (dev) { m_device = dev; }
        else
        {
            NS_LOG_ERROR("GatewayLorawanMac: Invalid device");
            return;
        }
    }

    Ptr<LoraNetDevice> loraDev = DynamicCast<LoraNetDevice>(m_device);
    if (!loraDev)
    {
        NS_LOG_ERROR("GatewayLorawanMac: Not a LoraNetDevice");
        return;
    }

    Ptr<Packet> packetCopy = packet->Copy();

    // Extract MAC header
    LorawanMacHeader macHdr;
    packetCopy->PeekHeader(macHdr);

    // Extract FHDR for address
    LoraDeviceAddress srcAddr;
    bool haveAddr = false;

    if (macHdr.IsUplink())
    {
        Ptr<Packet> tmp = packet->Copy();
        LorawanMacHeader tmpMac;
        tmp->RemoveHeader(tmpMac);

        LoraFrameHeader fhdr;
        if (tmp->PeekHeader(fhdr))
        {
            srcAddr = fhdr.GetAddress();
            haveAddr = true;
        }
    }

    // Extract freq & SF
    uint32_t freq = 0;
    uint8_t sf = 0;
    LoraTag tag;
    if (packetCopy->PeekPacketTag(tag))
    {
        freq = tag.GetFrequency();
        sf   = GetSfFromDataRate(tag.GetDataRate());
    }

    if (haveAddr && freq != 0 && sf != 0)
    {
        UpdateVcGroup(srcAddr, freq, sf);
    }

    BurstTag burstTag;
    if (packetCopy->PeekPacketTag(burstTag) && burstTag.GetBurst())
    {
        if (!m_inBurstMac)
        {
            m_inBurstMac = true;
            NS_LOG_INFO("Gateway entering Burst-MAC (node signaled burst)");
        }
    }


    if (freq != 0 && sf != 0)
        UpdateChannelStats(freq, sf, false);


    if (macHdr.IsUplink())
    {
        loraDev->Receive(packetCopy);
        m_receivedPacket(packet);

    }
}

void
GatewayLorawanMac::FailedReception(Ptr<const Packet> packet)
{
    LoraTag tag;
    if (packet->PeekPacketTag(tag))
    {
        uint32_t freq = tag.GetFrequency();
        uint8_t sf = GetSfFromDataRate(tag.GetDataRate());

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
    auto &group = m_vcGroups[key];

    group.insert(addr);

    uint32_t groupSize = group.size();
    m_groupSizes[key] = groupSize;

    uint32_t slot = addr.Get() % groupSize;
    m_slotAssignments[addr] = slot;
    
    g_pendingSchedules[addr] = std::make_tuple(slot, groupSize, sf);

    NS_LOG_INFO("VC Update: Device=" << addr.Get()
                                     << " Freq=" << freq
                                     << " SF=" << unsigned(sf)
                                     << " → slot=" << slot
                                     << " (group size=" << groupSize << ")");
    RecomputeScheduleForVc(freq, sf);
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
        NS_LOG_INFO("Gateway entering Burst-MAC due to high collisions");
    }
}


void
GatewayLorawanMac::SendScheduleToDevice(LoraDeviceAddress addr,
                                        uint32_t freq,
                                        uint8_t sf)
{
    auto key = std::make_pair(freq, sf);

    if (m_groupSizes.find(key) == m_groupSizes.end())
        return;

    uint32_t groupSize = m_groupSizes[key];
    uint32_t slot      = m_slotAssignments[addr];
    auto vcOverridesIt = m_slotOverrides.find(key);
    if (vcOverridesIt != m_slotOverrides.end())
    {
        auto &overrides = vcOverridesIt->second;
        auto it = overrides.find(addr);
        if (it != overrides.end())
        {
            slot = it->second;
        }
    }

    Ptr<Packet> down = Create<Packet>();

  
    LoraFrameHeader fhdr;
    fhdr.SetAsDownlink();
    fhdr.SetAddress(addr);
    fhdr.SetFCnt(0);
    down->AddHeader(fhdr);

    LorawanMacHeader macHdr;
    macHdr.SetMType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
    macHdr.SetMajor(1);
    down->AddHeader(macHdr);

    ScheduleTag s;
    s.Set(slot, groupSize, sf);
    down->AddPacketTag(s);

    LoraTxParameters params;
    params.sf = sf;
    params.bandwidthHz = 125000;
    params.codingRate = 1;
    params.headerDisabled = false;
    params.crcEnabled = true;
    params.nPreamble = 8;

    double txPower = 14;

    m_phy->Send(down, params, freq, txPower);

    NS_LOG_INFO("Gateway sending schedule → device=" << addr.Get()
                 << " slot=" << slot
                 << " groupSize=" << groupSize
                 << " sf=" << unsigned(sf));
}


void
GatewayLorawanMac::RecomputeScheduleForVc(uint32_t freq, uint8_t sf)
{
    auto key = std::make_pair(freq, sf);
    auto groupIt = m_vcGroups.find(key);
    if (groupIt == m_vcGroups.end())
        return;

    const auto &group = groupIt->second;
    const uint32_t groupSize = group.size();
    if (groupSize == 0)
        return;

    std::map<uint32_t, std::vector<LoraDeviceAddress>> occupancy;
    for (const auto &addr : group)
    {
        uint32_t baseSlot = addr.Get() % groupSize;
        occupancy[baseSlot].push_back(addr);
    }

    std::vector<uint32_t> freeSlots;
    std::vector<LoraDeviceAddress> collidingNodes;

    for (uint32_t s = 0; s < groupSize; ++s)
    {
        auto it = occupancy.find(s);
        if (it == occupancy.end())
        {
            freeSlots.push_back(s);
        }
        else if (it->second.size() > 1)
        {
            for (size_t i = 1; i < it->second.size(); ++i)
            {
                collidingNodes.push_back(it->second[i]);
            }
        }
    }

    auto &overrides = m_slotOverrides[key];
    overrides.clear();

    size_t idx = 0;
    while (idx < collidingNodes.size() && idx < freeSlots.size())
    {
        LoraDeviceAddress addr = collidingNodes[idx];
        uint32_t newSlot = freeSlots[idx];
        overrides[addr] = newSlot;
        m_slotAssignments[addr] = newSlot;
        NS_LOG_INFO("Task 5: Reassigned device=" << addr.Get()
                    << " to freeSlot=" << newSlot
                    << " in VC (" << freq << ", SF" << unsigned(sf) << ")");
        ++idx;
    }

    if (idx < collidingNodes.size())
    {
        NS_LOG_INFO("Task 5: Remaining collisions (" << (collidingNodes.size() - idx)
                    << ") due to lack of free slots in groupSize=" << groupSize);
    }
}


void
GatewayLorawanMac::StartBeacons(Time period)
{
    if (period.IsZero())
    {
        return;
    }
    m_beaconPeriod = period;
    m_beaconEvent = Simulator::Schedule(Seconds(2.0), &GatewayLorawanMac::SendBeacon, this);
    NS_LOG_INFO("Gateway starting beacons period=" << m_beaconPeriod.As(Time::S));
}

void
GatewayLorawanMac::SendBeacon()
{
    uint32_t freq = 0;
    uint8_t sf = 7;
    for (const auto &ch : m_channelHelper->GetRawChannelArray())
    {
        if (ch && ch->IsEnabledForUplink())
        {
            freq = ch->GetFrequency();
            break;
        }
    }
    if (freq == 0)
    {
        freq = 868100000;
    }

    Ptr<Packet> down = Create<Packet>();

    LoraFrameHeader fhdr;
    fhdr.SetAsDownlink();
    fhdr.SetAddress(LoraDeviceAddress(0)); 
    fhdr.SetFCnt(0);
    down->AddHeader(fhdr);

    LorawanMacHeader macHdr;
    macHdr.SetMType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
    macHdr.SetMajor(1);
    macHdr.SetBurst(true);
    down->AddHeader(macHdr);

    BeaconTag bt;
    bt.SetEpoch(Simulator::Now().GetMilliSeconds());
    down->AddPacketTag(bt);

    LoraTxParameters params;
    params.sf = sf;
    params.bandwidthHz = 125000;
    params.codingRate = 1;
    params.headerDisabled = false;
    params.crcEnabled = true;
    params.nPreamble = 8;
    params.lowDataRateOptimizationEnabled = false;

    double txPower = m_channelHelper->GetTxPowerForChannel(freq);

    m_channelHelper->AddEvent(LoraPhy::GetOnAirTime(down, params), freq);
    m_phy->Send(down, params, freq, txPower);

    NS_LOG_INFO("Gateway beacon sent freq=" << freq
                 << " sf=" << unsigned(sf)
                 << " epochMs=" << bt.GetEpoch());

    if (!m_beaconPeriod.IsZero())
    {
        m_beaconEvent = Simulator::Schedule(m_beaconPeriod, &GatewayLorawanMac::SendBeacon, this);
    }
}



} // namespace lorawan
} // namespace ns3
