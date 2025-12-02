/*
 * Copyright (c) 2017 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 */

#include "periodic-sender.h"

#include "end-device-lorawan-mac.h"
#include "lora-net-device.h"
#include "burst-tag.h"

#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/string.h"
#include "ns3/simulator.h"

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("PeriodicSender");

NS_OBJECT_ENSURE_REGISTERED(PeriodicSender);

TypeId
PeriodicSender::GetTypeId()
{
    static TypeId tid = TypeId("ns3::PeriodicSender")
                            .SetParent<Application>()
                            .AddConstructor<PeriodicSender>()
                            .SetGroupName("lorawan")
                            .AddAttribute("Interval",
                                          "The interval between packet sends of this app",
                                          TimeValue(Time(0)),
                                          MakeTimeAccessor(&PeriodicSender::GetInterval,
                                                           &PeriodicSender::SetInterval),
                                          MakeTimeChecker());
    // If you later want a PacketSizeRandomVariable Attribute, you can
    // re-enable and adapt the original code here.
    return tid;
}

PeriodicSender::PeriodicSender()
    : m_interval(Seconds(10)),
      m_initialDelay(Seconds(1)),
      m_basePktSize(10),
      m_pktSizeRV(nullptr),
      m_mac(nullptr),
      m_sendEvent(),
      m_lastTxTime(Seconds(0)),
      m_burstThreshold(Seconds(10)),   // threshold for "high rate"
      m_isBurst(false),
      m_forceBurst(false)
{
    NS_LOG_FUNCTION_NOARGS();
}

PeriodicSender::~PeriodicSender()
{
    NS_LOG_FUNCTION_NOARGS();
}

void
PeriodicSender::SetInterval(Time interval)
{
    NS_LOG_FUNCTION(this << interval);
    m_interval = interval;
}

Time
PeriodicSender::GetInterval() const
{
    NS_LOG_FUNCTION(this);
    return m_interval;
}

void
PeriodicSender::SetInitialDelay(Time delay)
{
    NS_LOG_FUNCTION(this << delay);
    m_initialDelay = delay;
}

void
PeriodicSender::SetPacketSizeRandomVariable(Ptr<RandomVariableStream> rv)
{
    NS_LOG_FUNCTION(this << rv);
    m_pktSizeRV = rv;
}

void
PeriodicSender::SetPacketSize(uint8_t size)
{
    NS_LOG_FUNCTION(this << unsigned(size));
    m_basePktSize = size;
}

void
PeriodicSender::SetForceBurst(bool force)
{
    m_forceBurst = force;
}

bool
PeriodicSender::GetForceBurst() const
{
    return m_forceBurst;
}

void
PeriodicSender::SendPacket()
{
    NS_LOG_FUNCTION(this);

    // ---- Build packet (original behavior) ----
    Ptr<Packet> packet;
    if (m_pktSizeRV)
    {
        int randomsize = m_pktSizeRV->GetInteger();
        packet = Create<Packet>(m_basePktSize + randomsize);
    }
    else
    {
        packet = Create<Packet>(m_basePktSize);
    }

    // ---- NEW: Simple burst detection on the node side ----
    Time now = Simulator::Now();
    if (m_lastTxTime != Seconds(0))
    {
        Time interval = now - m_lastTxTime;
        // If sending more frequently than threshold -> enter burst
        // Else -> exit burst (Task 6 switching back)
        if (interval < m_burstThreshold)
        {
            m_isBurst = true;
        }
        else
        {
            m_isBurst = false;
        }
    }
    m_lastTxTime = now;

    // Forced burst overrides timing-based detection
    if (m_forceBurst)
    {
        m_isBurst = true;
    }

    // Attach the burst flag as a packet tag
    BurstTag tag;
    tag.SetBurst(m_isBurst);
    packet->AddPacketTag(tag);

    NS_LOG_DEBUG("PeriodicSender sending packet, burst=" << m_isBurst
                                                         << ", size=" << packet->GetSize());

    // ---- Request confirmation BEFORE sending to trigger server replies ----
    m_mac->SetMType(LorawanMacHeader::CONFIRMED_DATA_UP);
    m_mac->Send(packet);

    // ---- Schedule next send ----
    m_sendEvent = Simulator::Schedule(m_interval, &PeriodicSender::SendPacket, this);
}

void
PeriodicSender::StartApplication()
{
    NS_LOG_FUNCTION(this);

    if (!m_mac)
    {
        // Assume there is only one LoraNetDevice on this node
        Ptr<LoraNetDevice> dev = DynamicCast<LoraNetDevice>(GetNode()->GetDevice(0));
        NS_ASSERT(dev);

        m_mac = DynamicCast<EndDeviceLorawanMac>(dev->GetMac());
        NS_ASSERT(m_mac);
    }

    Simulator::Cancel(m_sendEvent);
    NS_LOG_DEBUG("Starting PeriodicSender with initial delay "
                 << m_initialDelay.As(Time::S));
    m_sendEvent = Simulator::Schedule(m_initialDelay, &PeriodicSender::SendPacket, this);
}

void
PeriodicSender::StopApplication()
{
    NS_LOG_FUNCTION_NOARGS();
    Simulator::Cancel(m_sendEvent);
}

} // namespace lorawan
} // namespace ns3
