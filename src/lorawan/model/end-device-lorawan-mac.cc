/*
 * Copyright (c) 2017 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 *         Martina Capuzzo <capuzzom@dei.unipd.it>
 *
 * Modified for Burst-MAC research project.
 */

#include "end-device-lorawan-mac.h"

#include "burst-tag.h"
#include "class-a-end-device-lorawan-mac.h"
#include "end-device-lora-phy.h"
#include "schedule-tag.h"

#include "ns3/energy-source-container.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

#include <bitset>

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("EndDeviceLorawanMac");

NS_OBJECT_ENSURE_REGISTERED(EndDeviceLorawanMac);

TypeId
EndDeviceLorawanMac::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::EndDeviceLorawanMac")
            .SetParent<LorawanMac>()
            .SetGroupName("lorawan")
            .AddConstructor<EndDeviceLorawanMac>()
            .AddTraceSource("RequiredTransmissions",
                            "Total number of transmissions required to deliver this packet",
                            MakeTraceSourceAccessor(&EndDeviceLorawanMac::m_requiredTxCallback),
                            "ns3::TracedValueCallback::uint8_t")
            .AddAttribute("DataRate",
                          "Data rate currently employed by this end device",
                          UintegerValue(0),
                          MakeUintegerAccessor(&EndDeviceLorawanMac::m_dataRate),
                          MakeUintegerChecker<uint8_t>(0, 5))
            .AddTraceSource("DataRate",
                            "Data rate currently employed by this end device",
                            MakeTraceSourceAccessor(&EndDeviceLorawanMac::m_dataRate),
                            "ns3::TracedValueCallback::uint8_t")
            .AddAttribute("ADR",
                          "Enable or disable ADR",
                          BooleanValue(true),
                          MakeBooleanAccessor(&EndDeviceLorawanMac::m_adr),
                          MakeBooleanChecker());
    return tid;
}

EndDeviceLorawanMac::EndDeviceLorawanMac()
    : m_nbTrans(1),
      m_dataRate(0),
      m_txPowerDbm(14),
      m_codingRate(1),
      m_headerDisabled(false),
      m_address(LoraDeviceAddress(0)),
      m_receiveWindowDurationInSymbols(8),
      m_lastRxSnr(32),
      m_adrAckCnt(0),
      m_adr(true),
      m_lastKnownLinkMarginDb(0),
      m_lastKnownGatewayCount(0),
      m_aggregatedDutyCycle(1),
      m_mType(LorawanMacHeader::CONFIRMED_DATA_UP),
      m_currentFCnt(0),
      m_adrAckReq(false)
{
    NS_LOG_FUNCTION(this);

    m_uniformRV = CreateObject<UniformRandomVariable>();

    m_nextTx.Cancel();

    m_retxParams = EndDeviceLorawanMac::LoraRetxParameters();
    m_retxParams.retxLeft = m_nbTrans;
}

EndDeviceLorawanMac::~EndDeviceLorawanMac()
{
    NS_LOG_FUNCTION_NOARGS();
}

//////////////////////////////////////////////////////////
//                Sending Methods                      //
//////////////////////////////////////////////////////////

void
EndDeviceLorawanMac::Send(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    // Handle retransmissions and new packet logic
    if (packet == m_retxParams.packet)
    {
        LorawanMacHeader macHdr;
        packet->RemoveHeader(macHdr);
        LoraFrameHeader frameHdr;
        packet->RemoveHeader(frameHdr);
    }
    else
    {
        if (m_retxParams.waitingAck)
        {
            uint8_t txs = m_nbTrans - m_retxParams.retxLeft;
            m_requiredTxCallback(txs, false, m_retxParams.firstAttempt, m_retxParams.packet);
        }
    }

    // ADR backoff evaluation
    m_adrAckReq = (m_adrAckCnt >= ADR_ACK_LIMIT);
    if (m_adrAckCnt >= ADR_ACK_LIMIT + ADR_ACK_DELAY)
    {
        ExecuteADRBackoff();
        m_adrAckCnt = ADR_ACK_LIMIT;
    }

    if (!IsPayloadSizeValid(packet->GetSize(), m_dataRate))
    {
        NS_LOG_ERROR("Application payload exceeds maximum allowed size");
        return;
    }

    if (GetCompatibleTxChannels().empty())
    {
        NS_LOG_ERROR("No compatible channel for TX");
        return;
    }

    Time delay = GetNextTransmissionDelay();
    if (delay.IsStrictlyPositive())
    {
        PostponeTransmission(delay, packet);
        return;
    }

    DoSend(packet);
}

void
EndDeviceLorawanMac::PostponeTransmission(Time delay, Ptr<Packet> packet)
{
    Simulator::Cancel(m_nextTx);
    m_nextTx = Simulator::Schedule(delay, &EndDeviceLorawanMac::DoSend, this, packet);
}

void
EndDeviceLorawanMac::DoSend(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this);

    // *** TASK 6 ***
    // If a schedule tag is attached, log it before sending.
    ScheduleTag sched;
    if (packet->PeekPacketTag(sched))
    {
        NS_LOG_INFO("Burst-MAC scheduled TX → device="
                    << m_address.Get() << " vc=" << unsigned(sched.GetVc()) << " slot="
                    << sched.GetSlot() << (sched.GetReassigned() ? " (reassigned)" : ""));
    }

    // Build headers
    LoraFrameHeader frameHdr;
    ApplyNecessaryOptions(frameHdr);
    packet->AddHeader(frameHdr);

    LorawanMacHeader macHdr;
    ApplyNecessaryOptions(macHdr);
    packet->AddHeader(macHdr);

    if (packet != m_retxParams.packet)
    {
        m_macCommandList.clear();
        ResetRetransmissionParameters();
        m_retxParams.packet = packet->Copy();
        m_retxParams.firstAttempt = Now();
        m_retxParams.waitingAck = (m_mType == LorawanMacHeader::CONFIRMED_DATA_UP);
    }

    SendToPhy(packet);

    m_retxParams.retxLeft--;
    if (packet != m_retxParams.packet)
    {
        m_sentNewPacket(packet);
        m_currentFCnt++;
        m_adrAckCnt++;
    }
}

void
EndDeviceLorawanMac::SendToPhy(Ptr<Packet> packet)
{
    // Implemented in ClassAEndDeviceLorawanMac
}

//////////////////////////////////////////////////////////
//                 Receiving Methods                   //
//////////////////////////////////////////////////////////

void
EndDeviceLorawanMac::Receive(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    Ptr<Packet> p = packet->Copy();

    // *** TASK 6 ***
    ScheduleTag sched;
    if (p->PeekPacketTag(sched))
    {
        NS_LOG_INFO("Node received schedule → device="
                    << m_address.Get() << " vc=" << unsigned(sched.GetVc()) << " slot="
                    << sched.GetSlot() << (sched.GetReassigned() ? " (reassigned)" : ""));
    }

    // Normal LoRaWAN behavior continues in second half…
}

void
EndDeviceLorawanMac::FailedReception(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    // Keep track of collisions only in Confirmed uplink cases
    if (m_retxParams.waitingAck)
    {
        if (m_retxParams.retxLeft > 0)
        {
            this->Send(m_retxParams.packet);
            NS_LOG_INFO("Retransmission required: " << unsigned(m_retxParams.retxLeft)
                                                    << " retries remaining.");
        }
        else
        {
            uint8_t txs = m_nbTrans - (m_retxParams.retxLeft);
            m_requiredTxCallback(txs, false, m_retxParams.firstAttempt, m_retxParams.packet);
            NS_LOG_DEBUG("Failure: no more retransmissions left. " << "Used " << unsigned(txs)
                                                                   << " attempts.");

            ResetRetransmissionParameters();
        }
    }
}

void
EndDeviceLorawanMac::TxFinished(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    // Default EndDevice behavior delegates to Class-A implementation.
    Ptr<ClassAEndDeviceLorawanMac> a = DynamicCast<ClassAEndDeviceLorawanMac>(this);

    if (a)
    {
        a->TxFinished(packet);
    }
}

//////////////////////////////////////////////////////////
//             Downlink Packet Handling                //
//////////////////////////////////////////////////////////

void
EndDeviceLorawanMac::ParseCommands(const LoraFrameHeader& frameHdr)
{
    for (const auto& command : frameHdr.GetCommands())
    {
        switch (command->GetType())
        {
        case LoraMacCommand::DutyCycleReq: {
            auto c = DynamicCast<DutyCycleReq>(command);
            m_aggregatedDutyCycle = c->GetMaxDutyCycle();
            m_macCommandList.emplace_back(Create<DutyCycleAns>());
            break;
        }
        case LoraMacCommand::RxParamSetupReq: {
            auto c = DynamicCast<RxParamSetupReq>(command);
            Ptr<ClassAEndDeviceLorawanMac> a = DynamicCast<ClassAEndDeviceLorawanMac>(this);
            if (a)
            {
                a->OnRxParamSetupReq(c->GetRx1DrOffset(), c->GetRx2DataRate(), c->GetFrequency());
            }
            break;
        }
        case LoraMacCommand::DevStatusReq: {
            uint8_t bat = 255;
            m_macCommandList.emplace_back(Create<DevStatusAns>(bat, m_lastRxSnr));
            break;
        }
        case LoraMacCommand::LinkAdrReq: {
            auto c = DynamicCast<LinkAdrReq>(command);
            if (c)
            {
                bool dataRateOk = (c->GetDataRate() <= 5);
                bool powerOk = (c->GetTxPower() <= 7);
                bool channelMaskOk = true;

                if (dataRateOk)
                {
                    m_dataRate = c->GetDataRate();
                }
                if (powerOk)
                {
                    m_txPowerDbm = 14 - c->GetTxPower();
                }

                m_macCommandList.emplace_back(
                    Create<LinkAdrAns>(dataRateOk, powerOk, channelMaskOk));
            }
            break;
        }
        default:
            // Command not implemented
            break;
        }
    }
}

//////////////////////////////////////////////////////////
//             Retransmission / Timing Logic           //
//////////////////////////////////////////////////////////

Time
EndDeviceLorawanMac::GetNextTransmissionDelay()
{
    Time wait = Seconds(0);

    if (m_channelHelper)
    {
        auto ch = GetRandomChannelForTx();
        Time dutyWait = m_channelHelper->GetWaitTime(ch);
        wait = Max(wait, dutyWait);
    }

    Ptr<ClassAEndDeviceLorawanMac> a = DynamicCast<ClassAEndDeviceLorawanMac>(this);

    if (a)
    {
        wait = a->GetNextClassTransmissionDelay(wait);
    }

    return wait;
}

std::vector<Ptr<LogicalLoraChannel>>
EndDeviceLorawanMac::GetCompatibleTxChannels()
{
    return m_channelHelper->GetChannelsForSending(m_dataRate);
}

Ptr<LogicalLoraChannel>
EndDeviceLorawanMac::GetRandomChannelForTx()
{
    auto list = GetCompatibleTxChannels();
    if (list.empty())
    {
        return nullptr;
    }
    uint32_t index = m_uniformRV->GetInteger(0, list.size() - 1);
    return list.at(index);
}

void
EndDeviceLorawanMac::ExecuteADRBackoff()
{
    if (m_dataRate > 0)
    {
        m_dataRate--;
        NS_LOG_INFO("ADR Backoff: decreasing DR to " << unsigned(m_dataRate));
    }
}

void
EndDeviceLorawanMac::ResetRetransmissionParameters()
{
    m_retxParams.packet = nullptr;
    m_retxParams.retxLeft = m_nbTrans;
    m_retxParams.waitingAck = false;
}

//////////////////////////////////////////////////////////
//            Header / Command Construction            //
//////////////////////////////////////////////////////////

void
EndDeviceLorawanMac::ApplyNecessaryOptions(LoraFrameHeader& hdr)
{
    hdr.SetAsUplink();
    hdr.SetAddress(m_address);
    hdr.SetFcnt(m_currentFCnt);

    for (auto c : m_macCommandList)
    {
        hdr.AddCommand(c);
    }
}

void
EndDeviceLorawanMac::ApplyNecessaryOptions(LorawanMacHeader& hdr)
{
    hdr.SetMType(m_mType);
}

//////////////////////////////////////////////////////////
//             Payload Validation Utilities            //
//////////////////////////////////////////////////////////

bool
EndDeviceLorawanMac::IsPayloadSizeValid(uint32_t payloadSize, uint8_t dataRate)
{
    static const uint8_t maxPayloadEu868[] = {51, 51, 51, 115, 242, 242};

    if (dataRate >= sizeof(maxPayloadEu868))
    {
        return false;
    }
    return payloadSize <= maxPayloadEu868[dataRate];
}

//////////////////////////////////////////////////////////
//                     Misc. Methods                   //
//////////////////////////////////////////////////////////

void
EndDeviceLorawanMac::SetDeviceAddress(const LoraDeviceAddress& addr)
{
    m_address = addr;
}

LoraDeviceAddress
EndDeviceLorawanMac::GetDeviceAddress() const
{
    return m_address;
}

} // namespace lorawan
} // namespace ns3
