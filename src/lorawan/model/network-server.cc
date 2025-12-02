/*
 * Copyright (c) 2018 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Davide Magrin <magrinda@dei.unipd.it>
 *          Martina Capuzzo <capuzzom@dei.unipd.it>
 */

#include "network-server.h"
#include "beacon-tag.h"
#include "lora-net-device.h"
#include "lorawan-mac-header.h"
#include "lora-frame-header.h"

#include "class-a-end-device-lorawan-mac.h"
#include "lora-device-address.h"
#include "lora-frame-header.h"
#include "lorawan-mac-header.h"
#include "mac-command.h"
#include "network-status.h"

#include "ns3/log.h"
#include "ns3/net-device.h"
#include "ns3/node-container.h"
#include "ns3/packet.h"
#include "ns3/point-to-point-net-device.h"

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("NetworkServer");

NS_OBJECT_ENSURE_REGISTERED(NetworkServer);

TypeId
NetworkServer::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::NetworkServer")
            .SetParent<Application>()
            .AddConstructor<NetworkServer>()
            .AddTraceSource("ReceivedPacket",
                            "Trace fired when packet arrives at the network server",
                            MakeTraceSourceAccessor(&NetworkServer::m_receivedPacket),
                            "ns3::Packet::TracedCallback")
            .SetGroupName("lorawan");
    return tid;
}

NetworkServer::NetworkServer()
    : m_status(CreateObject<NetworkStatus>()),
      m_controller(CreateObject<NetworkController>(m_status)),
      m_scheduler(CreateObject<NetworkScheduler>(m_status, m_controller))
{
    NS_LOG_FUNCTION_NOARGS();
}

NetworkServer::~NetworkServer()
{
    NS_LOG_FUNCTION_NOARGS();
}

void
NetworkServer::StartApplication()
{
    NS_LOG_FUNCTION_NOARGS();
}

void
NetworkServer::StopApplication()
{
    NS_LOG_FUNCTION_NOARGS();
}

void
NetworkServer::AddGateway(Ptr<Node> gateway, Ptr<NetDevice> netDevice)
{
    NS_LOG_FUNCTION(this << gateway);

    Ptr<PointToPointNetDevice> p2pNetDevice;
    for (uint32_t i = 0; i < gateway->GetNDevices(); i++)
    {
        p2pNetDevice = DynamicCast<PointToPointNetDevice>(gateway->GetDevice(i));
        if (p2pNetDevice)
        {
            break;
        }
    }

    Ptr<GatewayLorawanMac> gwMac =
        DynamicCast<GatewayLorawanMac>(DynamicCast<LoraNetDevice>(gateway->GetDevice(0))->GetMac());
    NS_ASSERT(gwMac);

    Address gatewayAddress = p2pNetDevice->GetAddress();

    Ptr<GatewayStatus> gwStatus = CreateObject<GatewayStatus>(gatewayAddress, netDevice, gwMac);

    m_status->AddGateway(gatewayAddress, gwStatus);
}

void
NetworkServer::AddNodes(NodeContainer nodes)
{
    for (auto it = nodes.Begin(); it != nodes.End(); it++)
    {
        AddNode(*it);
    }
}

void
NetworkServer::AddNode(Ptr<Node> node)
{
    Ptr<LoraNetDevice> loraNetDevice;
    for (uint32_t i = 0; i < node->GetNDevices(); i++)
    {
        loraNetDevice = DynamicCast<LoraNetDevice>(node->GetDevice(i));
        if (loraNetDevice)
        {
            break;
        }
    }

    Ptr<ClassAEndDeviceLorawanMac> edLorawanMac =
        DynamicCast<ClassAEndDeviceLorawanMac>(loraNetDevice->GetMac());

    m_status->AddNode(edLorawanMac);
}

bool
NetworkServer::Receive(Ptr<NetDevice> device,
                       Ptr<const Packet> packet,
                       uint16_t protocol,
                       const Address& sender)
{
    NS_LOG_FUNCTION(this << packet << protocol << sender);

    m_receivedPacket(packet);

    // pass to scheduler + status + controller
    m_scheduler->OnReceivedPacket(packet);
    m_status->OnReceivedPacket(packet, sender);
    m_controller->OnNewPacket(packet);

    return true;
}

// Periodic beacon broadcast in burst mode (Task 6)
static Time g_beaconPeriod = Seconds(10);

void
NetworkServer::StartBurstBeacons()
{
    // Schedule first beacon
    Simulator::Schedule(Seconds(2.0), &NetworkServer::SendBeacon, this);
}

void
NetworkServer::SendBeacon()
{
    // Iterate gateways and send a small downlink with BeaconTag
    for (auto &kv : m_status->m_gatewayStatuses)
    {
        Ptr<GatewayStatus> gwStatus = kv.second;
        Address gwAddress = kv.first;

        Ptr<Packet> down = Create<Packet>();

        // Prepare headers (broadcast-like, but we attach mac+fhdr minimally)
        LoraFrameHeader fhdr;
        fhdr.SetAsDownlink();
        fhdr.SetAddress(LoraDeviceAddress(0)); // address 0 indicates beacon/general
        fhdr.SetFCnt(0);
        down->AddHeader(fhdr);

        LorawanMacHeader macHdr;
        macHdr.SetMType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
        macHdr.SetMajor(1);
        down->AddHeader(macHdr);

        BeaconTag bt;
        bt.SetEpoch(Simulator::Now().GetMilliSeconds());
        down->AddPacketTag(bt);

        // Use first RX window parameters of gateway region (approximate):
        LoraTag tag;
        tag.SetDataRate(5);           // DR5 (SF7) default beacon DR
        tag.SetFrequency(868100000);  // EU default first channel
        down->AddPacketTag(tag);

        gwStatus->GetNetDevice()->Send(down, gwAddress, 0x0800);
    }

    // Reschedule next beacon
    Simulator::Schedule(g_beaconPeriod, &NetworkServer::SendBeacon, this);
}

void
NetworkServer::AddComponent(Ptr<NetworkControllerComponent> component)
{
    m_controller->Install(component);
}

Ptr<NetworkStatus>
NetworkServer::GetNetworkStatus()
{
    return m_status;
}

} // namespace lorawan
} // namespace ns3
