/*
 * Copyright (c) 2017 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 */

 #ifndef GATEWAY_LORAWAN_MAC_H
 #define GATEWAY_LORAWAN_MAC_H
 
 #include "lora-device-address.h"
 #include "lora-tag.h"
 #include "lorawan-mac.h"
 
 #include "ns3/nstime.h"
 
 #include <map>
 #include <set>
 
 namespace ns3
 {
 namespace lorawan
 {
 
 /**
  * @ingroup lorawan
  *
  * Class representing the MAC layer of a LoRaWAN gateway.
  */
 class GatewayLorawanMac : public LorawanMac
 {
   public:
     /**
      *  Register this type.
      *  @return The object TypeId.
      */
     static TypeId GetTypeId();
 
     GatewayLorawanMac();           //!< Default constructor
     ~GatewayLorawanMac() override; //!< Destructor
 
     // Implementation of the LorawanMac interface
     void Send(Ptr<Packet> packet) override;
 
     /**
      * Check whether the underlying PHY layer of the gateway is currently transmitting.
      *
      * @return True if it is transmitting, false otherwise.
      */
     bool IsTransmitting();
 
     // Implementation of the LorawanMac interface
     void Receive(Ptr<const Packet> packet) override;
 
     // Implementation of the LorawanMac interface
     void FailedReception(Ptr<const Packet> packet) override;
 
     // Implementation of the LorawanMac interface
     void TxFinished(Ptr<const Packet> packet) override;
 
     /**
      * Return the next time at which we will be able to transmit on the specified frequency.
      *
      * @param frequencyHz The frequency value [Hz].
      * @return The next transmission time.
      */
     Time GetWaitTime(uint32_t frequencyHz);
 
   private:
     // -----------------------------
     // Task 2: Burst-MAC state
     // -----------------------------
     bool m_inBurstMac; //!< Whether the gateway is currently in Burst-MAC mode
 
     struct ChannelStats
     {
         uint32_t success = 0;
         uint32_t collisions = 0;
     };
 
     // key = (frequencyHz, SF)
     std::map<std::pair<uint32_t, uint8_t>, ChannelStats> m_channelStats;
 
     void UpdateChannelStats(uint32_t freq, uint8_t sf, bool collision);
 
     // -----------------------------
     // Task 3: Virtual Channel groups
     // VC group key = (frequency, SF)
     // -----------------------------
     std::map<std::pair<uint32_t, uint8_t>, std::set<LoraDeviceAddress>> m_vcGroups;
 
     void UpdateVcGroup(const LoraDeviceAddress& nodeAddr, uint32_t freq, uint8_t sf);
 
     // -----------------------------
     // Tasks 4 & 5: Scheduling state
     // -----------------------------
     struct SlotAssignment
     {
         uint8_t vc;
         uint16_t slot;
         bool reassigned;
     };
 
     // Per-node schedule
     std::map<LoraDeviceAddress, SlotAssignment> m_slotAssignments;
 
     // Track which nodes already had a schedule queued/logged
     std::set<LoraDeviceAddress> m_scheduleSent;
 
     bool m_scheduleComputed;
 
     // Compute schedules once we're in Burst-MAC
     void MaybeComputeSchedules();
 
     // Build per-VC, per-node slot assignments
     void ComputeSchedules();
 
     // Log/queue the schedule for a specific node
     void SendScheduleToNode(const LoraDeviceAddress& addr, uint32_t freq, uint8_t sf);
 };
 
 } // namespace lorawan
 
 } // namespace ns3
 
 #endif /* GATEWAY_LORAWAN_MAC_H */
 