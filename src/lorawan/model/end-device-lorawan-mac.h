/*
 * Modified version with schedule support (Tasks 4–6)
 * Compatible with newer ns-3 LoRaWAN module structure.
 */

 #ifndef END_DEVICE_LORAWAN_MAC_H
 #define END_DEVICE_LORAWAN_MAC_H
 
 #include "lora-device-address.h"
 #include "lora-frame-header.h"
 #include "lorawan-mac-header.h"
 #include "lorawan-mac.h"
 
 #include "ns3/random-variable-stream.h"
 #include "ns3/traced-value.h"
 #include "ns3/event-id.h"
 #include "ns3/time.h"
 
 #include "schedule-tag.h"   // <-- NEW for Task 4–6
 
 namespace ns3
 {
 namespace lorawan
 {
 
 class EndDeviceLorawanMac : public LorawanMac
 {
 public:
     static TypeId GetTypeId();
 
     EndDeviceLorawanMac();
     virtual ~EndDeviceLorawanMac();
 
     /////////////////////
     // Sending methods //
     /////////////////////
 
     void Send(Ptr<Packet> packet) override;
     virtual void DoSend(Ptr<Packet> packet);
     virtual void SendToPhy(Ptr<Packet> packet);
     virtual void PostponeTransmission(Time delay, Ptr<Packet> packet);
 
     ///////////////////////
     // Receiving methods //
     ///////////////////////
 
     void Receive(Ptr<const Packet> packet) override;
     void FailedReception(Ptr<const Packet> packet) override;
     void TxFinished(Ptr<const Packet> packet) override;
 
     /////////////////////////
     // Getters and Setters //
     /////////////////////////
 
     void ResetRetransmissionParameters();
     void SetUplinkAdrBit(bool adr);
     bool GetUplinkAdrBit() const;
 
     void SetMaxNumberOfTransmissions(uint8_t nbTrans);
     uint8_t GetMaxNumberOfTransmissions();
 
     void SetDataRate(uint8_t dataRate);
     uint8_t GetDataRate();
 
     double GetTransmissionPowerDbm();
     void SetTransmissionPowerDbm(double txPowerDbm);
 
     void SetDeviceAddress(LoraDeviceAddress address);
     LoraDeviceAddress GetDeviceAddress();
 
     uint8_t GetLastKnownLinkMarginDb() const;
     uint8_t GetLastKnownGatewayCount() const;
     double GetAggregatedDutyCycle();
 
     void ApplyNecessaryOptions(LoraFrameHeader& frameHeader);
     void ApplyNecessaryOptions(LorawanMacHeader& macHeader);
 
     void SetMType(LorawanMacHeader::MType mType);
     LorawanMacHeader::MType GetMType();
 
     void ParseCommands(LoraFrameHeader frameHeader);
 
     void OnLinkCheckAns(uint8_t margin, uint8_t gwCnt);
     void OnLinkAdrReq(uint8_t dataRate, uint8_t txPower, uint16_t chMask,
                       uint8_t chMaskCntl, uint8_t nbTrans);
 
     void OnDutyCycleReq(uint8_t maxDutyCycle);
 
     virtual void OnRxParamSetupReq(uint8_t rx1DrOffset,
                                    uint8_t rx2DataRate,
                                    double frequencyHz) = 0;
 
     void OnDevStatusReq();
     void OnNewChannelReq(uint8_t chIndex, uint32_t frequencyHz,
                          uint8_t minDataRate, uint8_t maxDataRate);
 
     void AddMacCommand(Ptr<MacCommand> command);
 
     /////////////////////////
     // NEW FOR TASKS 4–6   //
     /////////////////////////
 
     /** Store the last received schedule */
     void SetSchedule(uint8_t vc, uint16_t slot, bool reassigned)
     {
         m_hasSchedule = true;
         m_schedVc = vc;
         m_schedSlot = slot;
         m_schedReassigned = reassigned;
     }
 
     bool HasSchedule() const { return m_hasSchedule; }
     uint8_t GetScheduleVc() const { return m_schedVc; }
     uint16_t GetScheduleSlot() const { return m_schedSlot; }
     bool GetScheduleReassigned() const { return m_schedReassigned; }
 
 protected:
     struct RetxParams
     {
         Time firstAttempt;
         Ptr<Packet> packet = nullptr;
         bool waitingAck = false;
         uint8_t retxLeft;
     };
 
     uint8_t m_nbTrans;
     TracedValue<uint8_t> m_dataRate;
     TracedValue<double> m_txPowerDbm;
     uint8_t m_codingRate;
     bool m_headerDisabled;
     LoraDeviceAddress m_address;
 
     virtual Time GetNextClassTransmissionDelay(Time waitTime);
     std::vector<Ptr<LogicalLoraChannel>> GetCompatibleTxChannels();
     Time GetNextTransmissionDelay();
     void ExecuteADRBackoff();
     bool IsPayloadSizeValid(uint32_t size, uint8_t dr);
 
     bool m_adr;
     EventId m_nextTx;
     EventId m_nextRetx;
 
     double m_lastRxSnr;
     uint16_t m_adrAckCnt;
 
     TracedValue<uint8_t> m_lastKnownLinkMarginDb;
     TracedValue<uint8_t> m_lastKnownGatewayCount;
     TracedValue<double> m_aggregatedDutyCycle;
 
     LorawanMacHeader::MType m_mType;
     uint16_t m_currentFCnt;
 
     bool m_adrAckReq;
     Ptr<UniformRandomVariable> m_uniformRV;
     RetxParams m_retxParams;
 
     //////////////////////////////
     // TASK 4–6 DEVICE SCHEDULE //
     //////////////////////////////
 
     bool m_hasSchedule = false;
     uint8_t m_schedVc = 0;
     uint16_t m_schedSlot = 0;
     bool m_schedReassigned = false;
 };
 
 } // namespace lorawan
 } // namespace ns3
 
 #endif // END_DEVICE_LORAWAN_MAC_H
 