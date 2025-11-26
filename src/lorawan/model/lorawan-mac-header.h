/*
 * Copyright (c) 2017 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LORAWAN_MAC_HEADER_H
#define LORAWAN_MAC_HEADER_H

#include "ns3/header.h"

namespace ns3
{
namespace lorawan
{

/**
 * @ingroup lorawan
 *
 * This class represents the Mac header of a LoRaWAN packet.
 */
class LorawanMacHeader : public Header
{
  public:
    enum MType
    {
        JOIN_REQUEST = 0,
        JOIN_ACCEPT = 1,
        UNCONFIRMED_DATA_UP = 2,
        UNCONFIRMED_DATA_DOWN = 3,
        CONFIRMED_DATA_UP = 4,
        CONFIRMED_DATA_DOWN = 5,
        PROPRIETARY = 7
    };

    static TypeId GetTypeId();

    LorawanMacHeader();
    ~LorawanMacHeader() override;

    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetMType(enum MType mtype);
    uint8_t GetMType() const;

    void SetMajor(uint8_t major);
    uint8_t GetMajor() const;

    bool IsUplink() const;
    bool IsConfirmed() const;

    /** NEW: Burst flag **/
    void SetBurst(bool burst);
    bool GetBurst() const;

  private:
    uint8_t m_mtype;
    uint8_t m_major;

    /** NEW FIELD **/
    bool m_burst;
};

} // namespace lorawan
} // namespace ns3

#endif
