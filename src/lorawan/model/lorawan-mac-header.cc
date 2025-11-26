/*
 * Copyright (c) 2017 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "lorawan-mac-header.h"

#include "ns3/log.h"

#include <bitset>

namespace ns3 {
namespace lorawan {

NS_LOG_COMPONENT_DEFINE("LorawanMacHeader");

LorawanMacHeader::LorawanMacHeader()
    : m_major(0),
      m_burst(false)
{
}

LorawanMacHeader::~LorawanMacHeader()
{
}

TypeId
LorawanMacHeader::GetTypeId()
{
    static TypeId tid =
        TypeId("LorawanMacHeader").SetParent<Header>().AddConstructor<LorawanMacHeader>();
    return tid;
}

TypeId
LorawanMacHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
LorawanMacHeader::GetSerializedSize() const
{
    return 1; // still 1 byte total
}

void
LorawanMacHeader::Serialize(Buffer::Iterator start) const
{
    uint8_t header = 0;

    // MType = bits 7..5
    header |= (m_mtype << 5);

    // Burst flag = bit 3 (NEW)
    if (m_burst)
        header |= (1 << 3);

    // Major = bits 1..0
    header |= (m_major & 0b11);

    start.WriteU8(header);

    NS_LOG_DEBUG("Serialize MAC header = " << std::bitset<8>(header));
}

uint32_t
LorawanMacHeader::Deserialize(Buffer::Iterator start)
{
    uint8_t byte = start.ReadU8();

    m_mtype = byte >> 5;

    m_burst = (byte & (1 << 3)) != 0;

    m_major = byte & 0b11;

    return 1;
}

void
LorawanMacHeader::Print(std::ostream& os) const
{
    os << "MType=" << unsigned(m_mtype)
       << ", Major=" << unsigned(m_major)
       << ", Burst=" << m_burst;
}

void
LorawanMacHeader::SetMType(enum MType mtype)
{
    m_mtype = mtype;
}

uint8_t
LorawanMacHeader::GetMType() const
{
    return m_mtype;
}

void
LorawanMacHeader::SetMajor(uint8_t major)
{
    m_major = major;
}

uint8_t
LorawanMacHeader::GetMajor() const
{
    return m_major;
}

bool
LorawanMacHeader::IsUplink() const
{
    return (m_mtype == JOIN_REQUEST) || (m_mtype == UNCONFIRMED_DATA_UP) ||
           (m_mtype == CONFIRMED_DATA_UP);
}

bool
LorawanMacHeader::IsConfirmed() const
{
    return (m_mtype == CONFIRMED_DATA_DOWN) || (m_mtype == CONFIRMED_DATA_UP);
}

void
LorawanMacHeader::SetBurst(bool burst)
{
    m_burst = burst;
}

bool
LorawanMacHeader::GetBurst() const
{
    return m_burst;
}

} // namespace lorawan
} // namespace ns3
