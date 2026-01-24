#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/energy-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/random-variable-stream.h"

#include "event_emitter.h"
#include "ascon_crypto.h"
#include "snake_optimizer.h"
#include "memostp_protocol.h"
#include "crypto_app.h"
#include "node_monitor.h"
#include "metrics_collector.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MEMOSTPSimulation");

class EnergyModelHelper {
public:
    static void InstallEnergyModel(NodeContainer& nodes, double initialEnergy) {
        BasicEnergySourceHelper energySourceHelper;
        energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(initialEnergy));
        
        EnergySourceContainer energySources = energySourceHelper.Install(nodes);
        
        // Simple energy consumption model
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<BasicEnergySource> source = DynamicCast<BasicEnergySource>(energySources.Get(i));
            source->SetNode(nodes.Get(i));
        }
    }
    
    static double GetRemainingEnergy(NodeContainer& nodes, uint32_t nodeId) {
        if (nodeId < nodes.GetN()) {
            Ptr<EnergySource> source = nodes.Get(nodeId)->GetObject<EnergySource>();
            if (source) {
                return source->GetRemainingEnergy();
            }
        }
        return 0.0;
    }
};

class DeathChecker : public Object {
public:
    DeathChecker(NodeContainer& nodes, NodeMonitor& monitor, double checkInterval = 1.0)
        : m_nodes(nodes), m_monitor(monitor), m_checkInterval(checkInterval) {}
    
    void Start() {
        Simulator::Schedule(Seconds(m_checkInterval), &DeathChecker::CheckNodes, this);
    }
    
private:
    void CheckNodes() {
        double currentTime = Simulator::Now().GetSeconds();
        
        for (uint32_t i = 0; i < m_nodes.GetN(); ++i) {
            if (m_monitor.IsNodeAlive(i)) {
                double remainingEnergy = EnergyModelHelper::GetRemainingEnergy(m_nodes, i);
                if (remainingEnergy <= 0.05) { // Node dies when energy < 0.05J
                    m_monitor.CheckNodeDeath(i, currentTime, "Energy Depletion");
                    
                    // Disable node applications
                    Ptr<Node> node = m_nodes.Get(i);
                    for (uint32_t appIdx = 0; appIdx < node->GetNApplications(); ++appIdx) {
                        node->GetApplication(appIdx)->SetStopTime(Seconds(currentTime));
                    }
                }
            }
        }
        
        Simulator::Schedule(Seconds(m_checkInterval), &DeathChecker::CheckNodes, this);
    }
    
    NodeContainer& m_nodes;
    NodeMonitor& m_monitor;
    double m_checkInterval;
};

int main(int argc, char *argv[]) {
    EventEmitter& emitter = EventEmitter::Instance();
    emitter.SetSimulationStartTime();
    emitter.EmitEvent("simulation_start", 0);
    
    // Configuration parameters with defaults
    uint32_t nNodes = 25;
    double simulationTime = 60.0;
    double area = 400.0;
    int optimization_iters = 6;
    bool enable_optimization = true;
    bool enable_crypto = true;
    bool enable_node_death = true;
    double initialNodeEnergy = 5.0;
    double deathCheckInterval = 2.0;
    
    CommandLine cmd;
    cmd.AddValue("nNodes", "Number of nodes", nNodes);
    cmd.AddValue("simulationTime", "Simulation time", simulationTime);
    cmd.AddValue("area", "Simulation area (m)", area);
    cmd.AddValue("optIters", "Optimization iterations", optimization_iters);
    cmd.AddValue("enableOpt", "Enable optimization", enable_optimization);
    cmd.AddValue("enableCrypto", "Enable ASCON cryptography", enable_crypto);
    cmd.AddValue("enableDeath", "Enable node death tracking", enable_node_death);
    cmd.AddValue("initialEnergy", "Initial energy per node (J)", initialNodeEnergy);
    cmd.AddValue("deathCheck", "Death check interval (s)", deathCheckInterval);
    cmd.Parse(argc, argv);
    
    emitter.EmitEvent("config", 0, nNodes, static_cast<int>(simulationTime));
    
    std::cout << "\033[1;36m╔══════════════════════════════════════════════════════════════╗\033[0m" << std::endl;
    std::cout << "\033[1;36m║      ENHANCED MEMOSTP WITH NODE DEATH TRACKING             ║\033[0m" << std::endl;
    std::cout << "\033[1;36m╚══════════════════════════════════════════════════════════════╝\033[0m" << std::endl;
    
    // Initialize components
    NodeContainer nodes;
    nodes.Create(nNodes);
    
    // Initialize node monitor
    NodeMonitor nodeMonitor;
    nodeMonitor.InitializeNodes(nNodes, initialNodeEnergy);
    
    // Install energy model if death tracking is enabled
    if (enable_node_death) {
        EnergyModelHelper::InstallEnergyModel(nodes, initialNodeEnergy);
        std::cout << "🔋 Initial Node Energy: " << initialNodeEnergy << " J" << std::endl;
    }
    
    // Setup mobility (grid layout)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    
    double gridSpacing = 15.0;
    uint32_t gridSize = ceil(std::sqrt(nNodes));
    
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(20.0),
                                 "MinY", DoubleValue(20.0),
                                 "DeltaX", DoubleValue(gridSpacing),
                                 "DeltaY", DoubleValue(gridSpacing),
                                 "GridWidth", UintegerValue(gridSize),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.Install(nodes);
    
    // Update node positions in monitor
    for (uint32_t i = 0; i < nNodes; ++i) {
        Ptr<MobilityModel> mobilityModel = nodes.Get(i)->GetObject<MobilityModel>();
        if (mobilityModel) {
            Vector position = mobilityModel->GetPosition();
            nodeMonitor.UpdatePosition(i, position.x, position.y);
        }
    }
    
    std::cout << "📐 Network Layout: " << gridSize << "×" << gridSize 
              << " grid, spacing: " << gridSpacing << "m" << std::endl;
    
    // Setup WiFi
    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
        "Exponent", DoubleValue(3.0),
        "ReferenceDistance", DoubleValue(1.0),
        "ReferenceLoss", DoubleValue(46.677));
    
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.Set("TxPowerStart", DoubleValue(20.0));
    phy.Set("TxPowerEnd", DoubleValue(20.0));
    
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("DsssRate2Mbps"),
                                 "ControlMode", StringValue("DsssRate1Mbps"));
    
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);
    
    // Internet stack
    OlsrHelper olsr;
    olsr.Set("HelloInterval", TimeValue(Seconds(2.0)));
    
    Ipv4StaticRoutingHelper staticRouting;
    Ipv4ListRoutingHelper list;
    list.Add(staticRouting, 0);
    list.Add(olsr, 10);
    
    InternetStackHelper internet;
    internet.SetRoutingHelper(list);
    internet.Install(nodes);
    
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
    
    // MEMOSTP protocol
    EnhancedMEMOSTPProtocol memostp(nodes, optimization_iters);
    memostp.setCryptoEnabled(enable_crypto);
    
    if (enable_optimization) {
        memostp.initializeProtocol();
    }
    
    // Setup crypto applications
    if (enable_crypto) {
        uint16_t cryptoPort = 9999;
        uint32_t cryptoPairs = std::min<uint32_t>(8, nodes.GetN() / 2);
        
        for (uint32_t i = 0; i < cryptoPairs; i++) {
            uint32_t senderIdx = i * 2;
            uint32_t receiverIdx = (i * 2 + 1) % nodes.GetN();
            
            // Receiver
            Ptr<Socket> recvSocket = Socket::CreateSocket(nodes.Get(receiverIdx), UdpSocketFactory::GetTypeId());
            Ptr<CryptoTestApplication> recvApp = CreateObject<CryptoTestApplication>();
            recvApp->Setup(recvSocket, InetSocketAddress(Ipv4Address::GetAny(), cryptoPort), 
                          cryptoPort, 512, &memostp, true, receiverIdx);
            nodes.Get(receiverIdx)->AddApplication(recvApp);
            recvApp->SetStartTime(Seconds(1.0));
            recvApp->SetStopTime(Seconds(simulationTime - 1.0));
            
            // Sender
            Ptr<Socket> sendSocket = Socket::CreateSocket(nodes.Get(senderIdx), UdpSocketFactory::GetTypeId());
            Ptr<CryptoTestApplication> sendApp = CreateObject<CryptoTestApplication>();
            sendApp->Setup(sendSocket, InetSocketAddress(interfaces.GetAddress(receiverIdx), cryptoPort), 
                          cryptoPort, 512, &memostp, false, senderIdx);
            nodes.Get(senderIdx)->AddApplication(sendApp);
            sendApp->SetStartTime(Seconds(3.0 + i * 0.5));
            sendApp->SetStopTime(Seconds(simulationTime - 3.0));
        }
        
        std::cout << "📡 Setup " << cryptoPairs << " crypto pairs" << std::endl;
    }
    
    // Add echo traffic for testing
    uint16_t echoPort = 9;
    uint32_t numServers = std::max(1u, nNodes / 5);
    
    for (uint32_t i = 0; i < numServers; i++) {
        UdpEchoServerHelper echoServer(echoPort + i);
        ApplicationContainer serverApps = echoServer.Install(nodes.Get(i));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(simulationTime - 1.0));
    }
    
    for (uint32_t i = numServers; i < std::min<uint32_t>(nNodes, numServers * 4); i++) {
        uint32_t serverIndex = i % numServers;
        UdpEchoClientHelper echoClient(interfaces.GetAddress(serverIndex), echoPort + serverIndex);
        
        echoClient.SetAttribute("MaxPackets", UintegerValue(100));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.8)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));
        
        ApplicationContainer clientApps = echoClient.Install(nodes.Get(i));
        double startTime = 2.0 + (i - numServers) * 0.3;
        clientApps.Start(Seconds(startTime));
        clientApps.Stop(Seconds(simulationTime - 2.0));
    }
    
    // Start death checker if enabled
    if (enable_node_death) {
        DeathChecker deathChecker(nodes, nodeMonitor, deathCheckInterval);
        deathChecker.Start();
        std::cout << "🔍 Node death tracking enabled (check every " 
                  << deathCheckInterval << "s)" << std::endl;
    }
    
    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    
    // Metrics collector
    MetricsCollector metricsCollector;
    
    std::cout << "\n\033[1;33m⏳ SIMULATION STARTED...\033[0m" << std::endl;
    emitter.EmitEvent("simulation_running", 0);
    
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    
    // Collect all metrics
    metricsCollector.CollectFlowMetrics(monitor);
    
    // Calculate energy consumption
    double totalEnergy = 0.0;
    for (uint32_t i = 0; i < nNodes; ++i) {
        if (enable_node_death) {
            double remaining = EnergyModelHelper::GetRemainingEnergy(nodes, i);
            double consumed = initialNodeEnergy - remaining;
            totalEnergy += consumed;
            
            // Update node monitor
            nodeMonitor.UpdateEnergy(i, consumed);
        }
    }
    
    metricsCollector.UpdateEnergyMetrics(totalEnergy, nNodes);
    
    // Update crypto metrics
    if (enable_crypto) {
        metricsCollector.UpdateCryptoMetrics(
            memostp.getPacketsEncrypted(),
            memostp.getPacketsDecrypted()
        );
    }
    
    // Update death metrics
    double firstDeath = emitter.GetFirstNodeDeathTime();
    double lastDeath = emitter.GetLastNodeDeathTime();
    
    if (firstDeath > 0) {
        metricsCollector.UpdateNodeDeathMetrics(firstDeath, 0, nNodes);
        metricsCollector.UpdateNodeDeathMetrics(lastDeath, nNodes-1, nNodes);
    }
    
    // Display results
    emitter.PrintDeathStatistics();
    nodeMonitor.PrintNodeStatusTable();
    nodeMonitor.PrintNetworkLifetimeMetrics();
    
    if (enable_crypto) {
        memostp.printCryptoStats();
    }
    
    memostp.printProtocolStats();
    
    // Get comprehensive metrics
    metricsCollector.PrintComprehensiveMetrics();
    
    // Export metrics to CSV
    metricsCollector.ExportMetricsToCSV("simulation_metrics.csv");
    nodeMonitor.ExportNodeData("node_status.csv");
    
    // Display death statistics
    if (firstDeath > 0) {
        std::cout << "\n\033[1;31m💀 NODE DEATH STATISTICS SUMMARY:\033[0m" << std::endl;
        std::cout << "├─ First Node Death: " << std::fixed << std::setprecision(2) 
                  << firstDeath << "s" << std::endl;
        std::cout << "├─ Last Node Death:  " << lastDeath << "s" << std::endl;
        std::cout << "├─ Network Lifetime: " << (lastDeath - firstDeath) << "s" << std::endl;
        std::cout << "├─ Alive Nodes:      " << nodeMonitor.GetAliveNodeCount() 
                  << "/" << nNodes << std::endl;
        std::cout << "└─ Network Coverage: " << std::fixed << std::setprecision(1) 
                  << nodeMonitor.GetNetworkCoverage() << "%" << std::endl;
    }
    
    emitter.EmitEvent("simulation_complete", 0);
    
    std::cout << "\n\033[1;32m✅ Simulation completed successfully!\033[0m" << std::endl;
    std::cout << "\033[1;37m" << std::string(70, '=') << "\033[0m" << std::endl;
    
    Simulator::Destroy();
    return 0;
}