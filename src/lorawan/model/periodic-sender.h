/*
 * Copyright (c) 2017 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 */

#ifndef PERIODIC_SENDER_H
#define PERIODIC_SENDER_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"

namespace ns3
{
namespace lorawan
{

class EndDeviceLorawanMac; // forward declaration

/**
 * @ingroup lorawan
 *
 * This application periodically sends packets using the LoRaWAN MAC.
 */
class PeriodicSender : public Application
{
  public:
    static TypeId GetTypeId();

    PeriodicSender();
    ~PeriodicSender() override;

    void SetInterval(Time interval);
    Time GetInterval() const;

    void SetInitialDelay(Time delay);

    void SetPacketSizeRandomVariable(Ptr<RandomVariableStream> rv);
    void SetPacketSize(uint8_t size);

  protected:
    void StartApplication() override;
    void StopApplication() override;

    /// Actually send one packet and schedule the next.
    void SendPacket();

  private:
    // --- Original fields ---
    Time m_interval;                   //!< Inter-packet interval
    Time m_initialDelay;               //!< Delay before first packet
    uint8_t m_basePktSize;             //!< Base packet size (bytes)
    Ptr<RandomVariableStream> m_pktSizeRV; //!< Optional extra size RV

    Ptr<EndDeviceLorawanMac> m_mac;    //!< Pointer to MAC
    EventId m_sendEvent;               //!< Event for next send

    // --- NEW: Burst detection state ---
    Time m_lastTxTime;                 //!< Time of last transmission
    Time m_burstThreshold;             //!< Threshold for "burst" mode
    bool m_isBurst;                    //!< Whether we are currently in burst
};

} // namespace lorawan
} // namespace ns3

#endif // PERIODIC_SENDER_H
