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

#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"

#include <map>
#include <cstdint>

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

    /**
     * Check whether the gateway is currently in Burst-MAC mode.
     */
    bool IsInBurstMacMode() const
    {
        return m_inBurstMac;
    }

  private:
    // Key for a "virtual channel": (frequency, SF)
    struct ChannelKey
    {
        uint32_t frequency;
        uint8_t sf;

        bool operator<(const ChannelKey& other) const
        {
            if (frequency < other.frequency)
            {
                return true;
            }
            if (frequency > other.frequency)
            {
                return false;
            }
            return sf < other.sf;
        }
    };

    struct ChannelStats
    {
        uint32_t total = 0;
        uint32_t collisions = 0;
    };

    /**
     * Update collision statistics for a (frequency, SF) channel and,
     * if thresholds are exceeded, trigger Burst-MAC mode.
     *
     * @param frequencyHz Channel center frequency [Hz].
     * @param sf Spreading factor.
     * @param isCollision Whether this event was a failed reception (collision).
     */
    void UpdateChannelStats(uint32_t frequencyHz, uint8_t sf, bool isCollision);

    // Per-(freq,SF) stats
    std::map<ChannelKey, ChannelStats> m_channelStats;

    // Whether the gateway has entered Burst-MAC mode
    bool m_inBurstMac = false;

    // Collision-based trigger parameters
    double m_collisionThreshold = 0.3; // e.g., 30% collisions
    uint32_t m_minSamples = 10;        // Minimum samples before checking rate
};

} // namespace lorawan

} // namespace ns3
#endif /* GATEWAY_LORAWAN_MAC_H */
