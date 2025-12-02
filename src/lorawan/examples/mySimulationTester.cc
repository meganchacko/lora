#include "ns3/point-to-point-helper.h"
#include "ns3/lora-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/node-container.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/command-line.h"
#include "ns3/random-variable-stream.h"
#include "ns3/network-server-helper.h"
#include "ns3/forwarder-helper.h"
#include "ns3/periodic-sender-helper.h"
#include "ns3/basic-energy-source-helper.h"
#include "ns3/energy-source-container.h"
#include "ns3/lora-radio-energy-model-helper.h"
#include "ns3/lora-radio-energy-model.h"

#include <iostream>
#include <vector>
#include <unordered_map>

using namespace ns3;
using namespace lorawan;

NS_LOG_COMPONENT_DEFINE("LoraPdrSimulation");

// Metrics tracking
static uint64_t g_sent = 0;
static uint64_t g_recv = 0;
static std::unordered_map<uint64_t, ns3::Time> g_txTime;
static std::vector<double> g_latencies;

static void OnStartSending(Ptr<const Packet> pkt, uint32_t senderId)
{
    g_sent++;
    g_txTime[pkt->GetUid()] = Simulator::Now();
}

static void OnGatewayReceived(Ptr<const Packet> pkt, uint32_t gwId)
{
    g_recv++;
    auto it = g_txTime.find(pkt->GetUid());
    if (it != g_txTime.end())
    {
        double ms = (Simulator::Now() - it->second).GetMilliSeconds();
        g_latencies.push_back(ms);
        g_txTime.erase(it);
    }
}

int main(int argc, char* argv[])
{
    int nNodes = 100;
    int nGateways = 1;
    double radiusMeters = 2000;
    double simulationTimeSeconds = 70.0;
    Time appStopTime = Seconds(simulationTimeSeconds);
    double burstPct = 0.2; // default 20% of nodes forced to burst
    double burstPeriodSecs = 3.0; // default faster period for burst nodes

    bool verboseLogs = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nNodes", "Number of end devices", nNodes);
    cmd.AddValue("x", "Number of gateways", nGateways);
    cmd.AddValue("radius", "Radius of the deployment area in meters", radiusMeters);
    cmd.AddValue("verbose", "Enable verbose logging (slow for >500 nodes)", verboseLogs);
    cmd.AddValue("burstPct", "Fraction [0..1] of nodes forced into burst", burstPct);
    cmd.AddValue("burstPeriod", "Period (s) used by burst nodes", burstPeriodSecs);
    cmd.Parse(argc, argv);

    // No logging by default for maximum performance
    // Only enable when --verbose=1 is passed
    if (verboseLogs)
    {
        LogComponentEnable("LoraPdrSimulation", LOG_LEVEL_INFO);
        
        // Task 2: Burst Detection - Node-side
        LogComponentEnable("PeriodicSender", LOG_LEVEL_ALL);
        LogComponentEnable("EndDeviceLorawanMac", LOG_LEVEL_ALL);
        LogComponentEnable("ClassAEndDeviceLorawanMac", LOG_LEVEL_ALL);
        
        // Task 2: Burst Detection - Gateway-side
        LogComponentEnable("GatewayLorawanMac", LOG_LEVEL_ALL);
        LogComponentEnable("LoraInterferenceHelper", LOG_LEVEL_ALL);
        LogComponentEnable("NetworkScheduler", LOG_LEVEL_ALL);
        LogComponentEnable("NetworkController", LOG_LEVEL_ALL);
        
        // Task 3: Virtual Channels (VC) grouping
        LogComponentEnable("LogicalLoraChannelHelper", LOG_LEVEL_ALL);
        LogComponentEnable("LogicalLoraChannel", LOG_LEVEL_ALL);
        
        // Task 4: Hash-Based Scheduling
        LogComponentEnable("NetworkScheduler", LOG_LEVEL_ALL);  // Duplicate for emphasis
        
        // Packet/Frame Analysis
        LogComponentEnable("LorawanMacHeader", LOG_LEVEL_ALL);
        LogComponentEnable("LoraFrameHeader", LOG_LEVEL_ALL);
        LogComponentEnable("ScheduleTag", LOG_LEVEL_ALL);
        LogComponentEnable("LoraPhy", LOG_LEVEL_INFO);
        // Energy model debug (to confirm state changes and accumulation)
        LogComponentEnable("LoraRadioEnergyModel", LOG_LEVEL_DEBUG);
    }

    LoraPhyHelper phyHelper = LoraPhyHelper();
    LorawanMacHelper macHelper = LorawanMacHelper();
    LoraHelper helper = LoraHelper();
    MobilityHelper mobility;

    Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel>();
    loss->SetPathLossExponent(3.76);
    loss->SetReference(1, 7.7);

    Ptr<PropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel>();
    Ptr<LoraChannel> channel = CreateObject<LoraChannel>(loss, delay);
    
    phyHelper.SetChannel(channel);
    helper.EnablePacketTracking();

    NodeContainer endDevices;
    endDevices.Create(nNodes);

    mobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator",
                                  "rho", DoubleValue(radiusMeters),
                                  "X", DoubleValue(0.0),
                                  "Y", DoubleValue(0.0));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(endDevices);

    phyHelper.SetDeviceType(LoraPhyHelper::ED);
    macHelper.SetDeviceType(LorawanMacHelper::ED_A);
    uint8_t nwkId = 54;
    uint32_t nwkAddrBase = 1864;
    Ptr<LoraDeviceAddressGenerator> addrGen =
    CreateObject<LoraDeviceAddressGenerator>(nwkId, nwkAddrBase);
    macHelper.SetAddressGenerator(addrGen);
    helper.Install(phyHelper, macHelper, endDevices);

    NodeContainer gateways;
    gateways.Create(nGateways);

    Ptr<ListPositionAllocator> gwAllocator = CreateObject<ListPositionAllocator>();
    gwAllocator->Add(Vector(0.0, 1000.0, 15.0));
    // gwAllocator->Add(Vector(0.0, -1000.0, 15.0));
    mobility.SetPositionAllocator(gwAllocator);
    mobility.Install(gateways);

    phyHelper.SetDeviceType(LoraPhyHelper::GW);
    macHelper.SetDeviceType(LorawanMacHelper::GW);
    helper.Install(phyHelper, macHelper, gateways);

    LorawanMacHelper::SetSpreadingFactorsUp(endDevices, gateways, channel);

    // Install energy sources on end devices
    BasicEnergySourceHelper energy;
    energy.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(10000.0));
    EnergySourceContainer sources = energy.Install(endDevices);

    // Attach LoRa radio energy models with realistic current values
    LoraRadioEnergyModelHelper loraEnergy;
    loraEnergy.Set("StandbyCurrentA", DoubleValue(0.0014));      // 1.4 mA standby
    loraEnergy.Set("TxCurrentA", DoubleValue(0.028));            // 28 mA transmit
    loraEnergy.Set("RxCurrentA", DoubleValue(0.0112));           // 11.2 mA receive
    loraEnergy.Set("SleepCurrentA", DoubleValue(0.0000015));     // 1.5 µA sleep
    
    for (uint32_t i = 0; i < endDevices.GetN(); ++i)
    {
        Ptr<LoraNetDevice> dev = DynamicCast<LoraNetDevice>(endDevices.Get(i)->GetDevice(0));
        loraEnergy.Install(dev, sources.Get(i));
    }

    NetworkServerHelper nsHelper = NetworkServerHelper();
    ForwarderHelper forHelper = ForwarderHelper();
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    
    Ptr<Node> networkServer = CreateObject<Node>();
    P2PGwRegistration_t gwRegistration;
    for (uint32_t i = 0; i < gateways.GetN(); ++i)
    {
        auto container = p2p.Install(networkServer, gateways.Get(i));
        auto serverP2PNetDev = DynamicCast<PointToPointNetDevice>(container.Get(0));
        gwRegistration.emplace_back(serverP2PNetDev, gateways.Get(i));
    }

    nsHelper.SetGatewaysP2P(gwRegistration);
    nsHelper.SetEndDevices(endDevices);
    nsHelper.Install(networkServer);
    forHelper.Install(gateways);

    // Task 6: start beaconing directly on each gateway MAC
    for (auto gw = gateways.Begin(); gw != gateways.End(); ++gw)
    {
        Ptr<LoraNetDevice> dev = DynamicCast<LoraNetDevice>((*gw)->GetDevice(0));
        if (dev)
        {
            Ptr<GatewayLorawanMac> gwMac = DynamicCast<GatewayLorawanMac>(dev->GetMac());
            if (gwMac)
            {
                gwMac->StartBeacons(Seconds(10));
            }
        }
    }

    PeriodicSenderHelper appHelper = PeriodicSenderHelper();
    appHelper.SetPeriod(Seconds(6));
    appHelper.SetPacketSize(24);

    ApplicationContainer appContainer = appHelper.Install(endDevices);
    appContainer.Start(Time(0));
    appContainer.Stop(appStopTime);

    // Force a percentage of nodes into burst mode by toggling their PeriodicSender
    uint32_t burstCount = std::min<uint32_t>(endDevices.GetN(), (uint32_t)std::round(burstPct * endDevices.GetN()));
    for (uint32_t i = 0; i < burstCount && i < appContainer.GetN(); ++i)
    {
        Ptr<Application> app = appContainer.Get(i);
        Ptr<lorawan::PeriodicSender> ps = DynamicCast<lorawan::PeriodicSender>(app);
        if (ps)
        {
            ps->SetForceBurst(true);
            ps->SetInterval(Seconds(burstPeriodSecs));
        }
    }

    // Connect traces for metrics collection
    for (auto node = endDevices.Begin(); node != endDevices.End(); ++node)
    {
        DynamicCast<LoraNetDevice>((*node)->GetDevice(0))
            ->GetPhy()
            ->TraceConnectWithoutContext("StartSending", MakeCallback(&OnStartSending));
    }

    for (auto node = gateways.Begin(); node != gateways.End(); ++node)
    {
        DynamicCast<LoraNetDevice>((*node)->GetDevice(0))
            ->GetPhy()
            ->TraceConnectWithoutContext("ReceivedPacket", MakeCallback(&OnGatewayReceived));
    }

    Simulator::Stop(appStopTime + Hours(1));
    Simulator::Run();

    // Compute metrics (do this BEFORE Destroy to keep objects alive)
    double prr = (g_sent > 0) ? (double)g_recv / (double)g_sent : 0.0;
    
    double avgLatency = 0.0;
    if (!g_latencies.empty())
    {
        double sum = 0.0;
        for (double v : g_latencies) sum += v;
        avgLatency = sum / g_latencies.size();
    }
    
    double totalEnergyJ = 0.0;
    for (uint32_t i = 0; i < endDevices.GetN(); ++i)
    {
        auto src = sources.Get(i);
        DeviceEnergyModelContainer models = src->FindDeviceEnergyModels("ns3::LoraRadioEnergyModel");
        for (auto it = models.Begin(); it != models.End(); ++it)
        {
            auto dem = DynamicCast<LoraRadioEnergyModel>(*it);
            if (dem)
            {
                totalEnergyJ += dem->GetTotalEnergyConsumption();
            }
        }
    }
    double avgEnergyPerPacketJ = (g_sent > 0) ? (totalEnergyJ / (double)g_sent) : 0.0;

    Simulator::Destroy();

    // Print results
    std::cout << "\n========== Simulation Results ==========\n";
    std::cout << "Packets sent: " << g_sent << "\n";
    std::cout << "Packets received: " << g_recv << "\n";
    std::cout << "Packet Reception Ratio (PRR): " << (prr * 100.0) << "%\n";
    std::cout << "Average latency (ms): " << avgLatency << "\n";
    std::cout << "Total radio energy (J): " << totalEnergyJ << "\n";
    std::cout << "Avg energy per packet (J): " << avgEnergyPerPacketJ << "\n";
    std::cout << "========================================\n";

    return 0;
}