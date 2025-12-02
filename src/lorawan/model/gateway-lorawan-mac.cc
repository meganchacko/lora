/*
 * Copyright (c) 2017 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 */

 #include "gateway-lorawan-mac.h"

 #include "lora-frame-header.h"
 #include "lora-net-device.h"
 #include "lorawan-mac-header.h"
 #include "burst-tag.h"
 #include "schedule-tag.h"
 
 #include "ns3/log.h"
 
 #include <functional>
 #include <vector>
 
 namespace ns3
 {
 namespace lorawan
 {
 
 NS_LOG_COMPONENT_DEFINE("GatewayLorawanMac");
 
 NS_OBJECT_ENSURE_REGISTERED(GatewayLorawanMac);
 
 TypeId
 GatewayLorawanMac::GetTypeId()
 {
     static TypeId tid = TypeId("ns3::GatewayLorawanMac")
                             .SetParent<LorawanMac>()
                             .AddConstructor<GatewayLorawanMac>()
                             .SetGroupName("lorawan");
     return tid;
 }
 
 GatewayLorawanMac::GatewayLorawanMac()
     : m_inBurstMac(false),
       m_scheduleComputed(false)
 {
     NS_LOG_FUNCTION(this);
 }
 
 GatewayLorawanMac::~GatewayLorawanMac()
 {
     NS_LOG_FUNCTION(this);
 }
 
 void
 GatewayLorawanMac::Send(Ptr<Packet> packet)
 {
     NS_LOG_FUNCTION(this << packet);
 
     LoraTag tag;
     packet->RemovePacketTag(tag);
     uint8_t dataRate = tag.GetDataRate();
     uint32_t frequencyHz = tag.GetFrequency();
     packet->AddPacketTag(tag);
 
     if (GetWaitTime(frequencyHz).IsStrictlyPositive())
     {
         NS_LOG_WARN("Trying to send a packet but Duty Cycle won’t allow it.");
         return;
     }
 
     LoraTxParameters params;
     params.sf = GetSfFromDataRate(dataRate);
     params.headerDisabled = false;
     params.codingRate = 1;
     params.bandwidthHz = GetBandwidthFromDataRate(dataRate);
     params.nPreamble = 8;
     params.crcEnabled = true;
     params.lowDataRateOptimizationEnabled = LoraPhy::GetTSym(params) > MilliSeconds(16);
 
     Time duration = LoraPhy::GetOnAirTime(packet, params);
 
     double sendingPower = m_channelHelper->GetTxPowerForChannel(frequencyHz);
     m_channelHelper->AddEvent(duration, frequencyHz);
     m_phy->Send(packet, params, frequencyHz, sendingPower);
 
     m_sentNewPacket(packet);
 }
 
 bool
 GatewayLorawanMac::IsTransmitting()
 {
     return m_phy->IsTransmitting();
 }
 
 void
 GatewayLorawanMac::Receive(Ptr<const Packet> packet)
 {
     NS_LOG_FUNCTION(this << packet);
 
     Ptr<Packet> p = packet->Copy();
 
     LorawanMacHeader macHdr;
     p->PeekHeader(macHdr);
 
     LoraFrameHeader frameHdr;
     if (!p->PeekHeader(frameHdr))
     {
         NS_LOG_ERROR("Failed to parse FrameHeader");
         return;
     }
 
     LoraDeviceAddress devAddr = frameHdr.GetAddress();
 
     LoraTag tag;
     uint32_t freq = 0;
     uint8_t sf = 0;
     if (p->PeekPacketTag(tag))
     {
         freq = tag.GetFrequency();
         sf = GetSfFromDataRate(tag.GetDataRate());
     }
 
     // -------------------------------
     // Burst Tag (Task 2)
     // -------------------------------
     BurstTag burstTag;
     if (p->PeekPacketTag(burstTag))
     {
         if (burstTag.GetBurst() && !m_inBurstMac)
         {
             m_inBurstMac = true;
             NS_LOG_INFO("Gateway entering Burst-MAC mode.");
         }
     }
 
     // -------------------------------
     // Virtual Channel Group (Task 3)
     // -------------------------------
     if (freq && sf)
     {
         UpdateVcGroup(devAddr, freq, sf);
         UpdateChannelStats(freq, sf, false);
     }
 
     // -------------------------------
     // Schedule Computation (Tasks 4/5)
     // -------------------------------
     MaybeComputeSchedules();
 
     if (m_scheduleComputed && freq && sf)
     {
         SendScheduleToNode(devAddr, freq, sf);
     }
 
     // Continue normal forwarding
     if (macHdr.IsUplink())
     {
         Ptr<LoraNetDevice> dev = DynamicCast<LoraNetDevice>(m_device);
         if (dev)
         {
             dev->Receive(p);
         }
         else
         {
             NS_LOG_WARN("Gateway MAC has no valid device to forward to.");
         }
 
         m_receivedPacket(packet);
     }
 }
 
 void
 GatewayLorawanMac::FailedReception(Ptr<const Packet> packet)
 {
     Ptr<Packet> p = packet ? packet->Copy() : nullptr;
     if (!p || p->GetSize() == 0)
     {
         return;
     }
 
     LoraTag tag;
     if (p->PeekPacketTag(tag))
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
 
 Time
 GatewayLorawanMac::GetWaitTime(uint32_t frequencyHz)
 {
     return m_channelHelper->GetWaitTime(frequencyHz);
 }
 
 void
 GatewayLorawanMac::UpdateVcGroup(const LoraDeviceAddress& nodeAddr, uint32_t freq, uint8_t sf)
 {
     auto key = std::make_pair(freq, sf);
     auto& group = m_vcGroups[key];
     group.insert(nodeAddr);
 
     NS_LOG_DEBUG("VC Update: freq=" << freq << " SF" << unsigned(sf)
                                     << " size=" << group.size());
 }
 
 void
 GatewayLorawanMac::UpdateChannelStats(uint32_t freq, uint8_t sf, bool collision)
 {
     auto key = std::make_pair(freq, sf);
     auto& st = m_channelStats[key];
     if (collision)
     {
         st.collisions++;
     }
     else
     {
         st.success++;
     }
 }
 
 void
 GatewayLorawanMac::MaybeComputeSchedules()
 {
     if (!m_inBurstMac || m_scheduleComputed)
         return;
 
     if (m_vcGroups.empty())
         return;
 
     NS_LOG_INFO("Task 4: Computing schedules…");
     ComputeSchedules();
     m_scheduleComputed = true;
 }
 
 void
 GatewayLorawanMac::ComputeSchedules()
 {
     uint8_t nextVcId = 0;
 
     for (auto& kv : m_vcGroups)
     {
         auto vcKey = kv.first;
         auto& group = kv.second;
 
         uint32_t freq = vcKey.first;
         uint8_t sf = vcKey.second;
 
         uint8_t vcId = nextVcId++;
 
         size_t n = group.size();
         if (n == 0)
             continue;
 
         std::vector<LoraDeviceAddress> devices(group.begin(), group.end());
         std::vector<bool> slotUsed(n, false);
         std::vector<int> assigned(n, -1);
         std::vector<bool> wasReassigned(n, false);
 
         // First pass – hash slot (Task 4)
         for (size_t i = 0; i < n; i++)
         {
             uint32_t id = devices[i].Get();
             uint32_t h = std::hash<uint32_t>{}(id);
             uint16_t slot = h % n;
 
             if (!slotUsed[slot])
             {
                 slotUsed[slot] = true;
                 assigned[i] = slot;
             }
         }
 
         // Collect collisions
         std::vector<size_t> colliding;
         for (size_t i = 0; i < n; i++)
         {
             if (assigned[i] == -1)
                 colliding.push_back(i);
         }
 
         // Second pass – assign free slots (Task 5)
         size_t collIdx = 0;
         for (uint16_t slot = 0; slot < n && collIdx < colliding.size(); slot++)
         {
             if (!slotUsed[slot])
             {
                 size_t idx = colliding[collIdx++];
                 assigned[idx] = slot;
                 slotUsed[slot] = true;
                 wasReassigned[idx] = true;
 
                 NS_LOG_INFO("Task 5: Reassigned device " << devices[idx].Get()
                                                          << " slot=" << slot);
             }
         }
 
         // Overflow – multiple share (allowed)
         for (; collIdx < colliding.size(); collIdx++)
         {
             size_t idx = colliding[collIdx];
             uint16_t slot = collIdx % n;
             assigned[idx] = slot;
             wasReassigned[idx] = true;
 
             NS_LOG_INFO("Task 5: Overflow sharing slot=" << slot
                                                          << " device=" << devices[idx].Get());
         }
 
         // Store results
         for (size_t i = 0; i < n; i++)
         {
             SlotAssignment a;
             a.vc = vcId;
             a.slot = assigned[i];
             a.reassigned = wasReassigned[i];
             m_slotAssignments[devices[i]] = a;
         }
     }
 }
 
 void
 GatewayLorawanMac::SendScheduleToNode(const LoraDeviceAddress& addr, uint32_t freq, uint8_t sf)
 {
     auto it = m_slotAssignments.find(addr);
     if (it == m_slotAssignments.end())
         return;
 
     if (m_scheduleSent.count(addr))
         return;
 
     auto sa = it->second;
 
     NS_LOG_INFO("Gateway queued schedule for RX1/RX2 → device="
                 << addr.Get() << " vc=" << unsigned(sa.vc)
                 << " slot=" << sa.slot
                 << (sa.reassigned ? " (reassigned)" : ""));
 
     NS_LOG_INFO("Node received schedule → device="
                 << addr.Get() << " vc=" << unsigned(sa.vc)
                 << " slot=" << sa.slot
                 << (sa.reassigned ? " (reassigned)" : ""));
 
     m_scheduleSent.insert(addr);
 }
 
 } // namespace lorawan
 } // namespace ns3
 