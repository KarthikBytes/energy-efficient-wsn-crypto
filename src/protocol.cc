// memostp-simulation-fixed-full.cc
// NS-3.40 MEMOSTP Simulation with OLSR routing, fixed WiFi standard, and correct FlowMonitor stats
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
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <random>
#include <iomanip>

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("MEMOSTPSimulation");

// Global variables for fitness evaluation
uint32_t g_totalTxPackets = 0;
uint32_t g_totalRxPackets = 0;
double g_totalEnergyConsumed = 0.0;

///////////////////////////
// Enhanced Snake Optimizer Class //
///////////////////////////
class EnhancedSnakeOptimizer {
private:
    struct Snake {
        std::vector<double> position;
        double fitness = std::numeric_limits<double>::max();
        std::vector<double> velocity;
        std::vector<double> personal_best_position;
        double personal_best_fitness = std::numeric_limits<double>::max();
    };

    std::vector<Snake> population;
    int populationSize, dimensions;
    double c1, c2, w_min, w_max;
    std::mt19937 rng;
    Snake global_best;
    int iteration_count;

public:
    EnhancedSnakeOptimizer(int popSize, int dim)
        : populationSize(popSize), dimensions(dim), c1(2.05), c2(2.05),
          w_min(0.4), w_max(0.9), rng(std::random_device{}()), iteration_count(0)
    {
        population.resize(populationSize);
        for (auto &snake : population) {
            snake.position.resize(dimensions);
            snake.velocity.resize(dimensions);
            snake.personal_best_position.resize(dimensions);

            for (int i = 0; i < dimensions; i++) {
                snake.position[i] = std::uniform_real_distribution<double>(0.1, 0.9)(rng);
                snake.velocity[i] = std::uniform_real_distribution<double>(-0.1, 0.1)(rng);
                snake.personal_best_position[i] = snake.position[i];
            }
            snake.personal_best_fitness = std::numeric_limits<double>::max();
        }
        global_best = population[0];
    }

    double get_inertia_weight() const {
        return w_max - (w_max - w_min) * (iteration_count / 100.0);
    }

    std::vector<double> optimize(int iterations) {
        for (int iter = 0; iter < iterations; iter++) {
            iteration_count = iter;
            double w = get_inertia_weight();

            for (auto &snake : population) {
                // Update velocity
                std::uniform_real_distribution<double> dist(0.0, 1.0);
                for (int i = 0; i < dimensions; i++) {
                    double r1 = dist(rng);
                    double r2 = dist(rng);

                    snake.velocity[i] = w * snake.velocity[i] +
                        c1 * r1 * (snake.personal_best_position[i] - snake.position[i]) +
                        c2 * r2 * (global_best.position[i] - snake.position[i]);

                    snake.velocity[i] = std::max(-0.2, std::min(0.2, snake.velocity[i]));
                }

                // Update position
                for (int i = 0; i < dimensions; i++) {
                    snake.position[i] += snake.velocity[i];
                    snake.position[i] = std::max(0.01, std::min(0.99, snake.position[i]));
                }

                // Fitness evaluation
                double pdr = (g_totalTxPackets > 0) ? (double)g_totalRxPackets / g_totalTxPackets : 0.0;
                double energy_eff = (g_totalEnergyConsumed > 0) ? g_totalRxPackets / g_totalEnergyConsumed : 0.0;

                snake.fitness = -(0.7 * pdr + 0.3 * energy_eff);

                if (snake.fitness < snake.personal_best_fitness) {
                    snake.personal_best_fitness = snake.fitness;
                    snake.personal_best_position = snake.position;
                }

                if (snake.fitness < global_best.fitness) {
                    global_best = snake;
                }
            }
        }
        return global_best.position;
    }

    double getBestEnergyWeight(const std::vector<double> &params) const { return params[0]; }
    double getBestPowerControl(const std::vector<double> &params) const { return params[1]; }
    double getBestSleepRatio(const std::vector<double> &params) const { return params[2]; }
};

///////////////////////////
// Enhanced MEMOSTP Protocol Class //
///////////////////////////
class EnhancedMEMOSTPProtocol {
private:
    NodeContainer nodes;
    EnhancedSnakeOptimizer optimizer;
    std::vector<double> optimizedParams;
    int optimization_iterations;

public:
    EnhancedMEMOSTPProtocol(NodeContainer &nodeContainer, int opt_iters = 20)
        : nodes(nodeContainer), optimizer(20, 3), optimization_iterations(opt_iters) {}

    void initializeProtocol() {
        NS_LOG_INFO("Initializing Enhanced MEMOSTP Protocol");
        optimizedParams = optimizer.optimize(optimization_iterations);
        NS_LOG_INFO("MEMOSTP Protocol Initialized with optimized parameters");
    }

    double getEnergyWeight() const { return optimizedParams.size() > 0 ? optimizedParams[0] : 0.6; }
    double getPowerControl() const { return optimizedParams.size() > 1 ? optimizedParams[1] : 0.8; }
    double getSleepRatio() const { return optimizedParams.size() > 2 ? optimizedParams[2] : 0.3; }
};

///////////////////////////
// Main Simulation       //
///////////////////////////
int main(int argc, char *argv[]) {
    uint32_t nNodes = 100;
    double simulationTime = 50.0;
    double area = 800.0;
    int optimization_iters = 20;
    bool enable_optimization = true;

    CommandLine cmd;
    cmd.AddValue("nNodes", "Number of nodes", nNodes);
    cmd.AddValue("simulationTime", "Simulation time", simulationTime);
    cmd.AddValue("area", "Simulation area (m)", area);
    cmd.AddValue("optIters", "Optimization iterations", optimization_iters);
    cmd.AddValue("enableOpt", "Enable optimization", enable_optimization);
    cmd.Parse(argc, argv);

    LogComponentEnable("MEMOSTPSimulation", LOG_LEVEL_INFO);

    NS_LOG_INFO("Starting Enhanced MEMOSTP Simulation with " << nNodes << " nodes");

    NodeContainer nodes;
    nodes.Create(nNodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // WiFi channel and PHY
    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
        "Exponent", DoubleValue(3.0),
        "ReferenceDistance", DoubleValue(1.0),
        "ReferenceLoss", DoubleValue(46.677));

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.Set("TxPowerStart", DoubleValue(10.0));
    phy.Set("TxPowerEnd", DoubleValue(10.0));
    phy.Set("TxGain", DoubleValue(2.0));
    phy.Set("RxGain", DoubleValue(2.0));
    phy.Set("RxNoiseFigure", DoubleValue(7.0));
    phy.Set("CcaEdThreshold", DoubleValue(-62.0));

    // WiFi configuration (fixed)
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs7"),
                                 "ControlMode", StringValue("HtMcs0"));

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac", "QosSupported", BooleanValue(false));

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Internet stack
    OlsrHelper olsr;
    olsr.Set("HelloInterval", TimeValue(Seconds(2.0)));
    olsr.Set("TcInterval", TimeValue(Seconds(5.0)));

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
    if (enable_optimization) {
        memostp.initializeProtocol();
    }

    // Energy model
    BasicEnergySourceHelper basicSourceHelper;
    basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(50.0));
    EnergySourceContainer sources = basicSourceHelper.Install(nodes);

    WifiRadioEnergyModelHelper radioEnergyHelper;
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.280 * memostp.getPowerControl()));
    radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.020));
    radioEnergyHelper.Set("IdleCurrentA", DoubleValue(0.001));
    radioEnergyHelper.Set("SleepCurrentA", DoubleValue(0.00002));

    DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(devices, sources);

    // Applications
    uint16_t port = 9;
    uint32_t numServers = std::max(1u, nNodes / 8);

    for (uint32_t i = 0; i < numServers; i++) {
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApps = echoServer.Install(nodes.Get(i));
        serverApps.Start(Seconds(2.0));
        serverApps.Stop(Seconds(simulationTime - 2.0));
    }

    Ptr<UniformRandomVariable> randomStart = CreateObject<UniformRandomVariable>();
    randomStart->SetAttribute("Min", DoubleValue(3.0));
    randomStart->SetAttribute("Max", DoubleValue(8.0));

    for (uint32_t i = numServers; i < nNodes; i++) {
        uint32_t serverIndex = i % numServers;
        UdpEchoClientHelper echoClient(interfaces.GetAddress(serverIndex), port);

        echoClient.SetAttribute("MaxPackets", UintegerValue(50));
        echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(800 + (i % 10) * 100)));
        echoClient.SetAttribute("PacketSize", UintegerValue(256));

        ApplicationContainer clientApps = echoClient.Install(nodes.Get(i));
        double startTime = randomStart->GetValue() + (i - numServers) * 0.1;
        clientApps.Start(Seconds(startTime));
        clientApps.Stop(Seconds(simulationTime - 5.0));
    }

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Energy calculation
    g_totalEnergyConsumed = 0;
    for (uint32_t i = 0; i < sources.GetN(); ++i) {
        Ptr<BasicEnergySource> source = DynamicCast<BasicEnergySource>(sources.Get(i));
        if (source) {
            g_totalEnergyConsumed += source->GetInitialEnergy() - source->GetRemainingEnergy();
        }
    }

    // Flow statistics
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    g_totalTxPackets = 0;
    g_totalRxPackets = 0;
    uint32_t totalLostPackets = 0;
    double totalDelay = 0, totalThroughput = 0;
    uint32_t flows_with_packets = 0;

    for (auto &flow : stats) {
        g_totalTxPackets += flow.second.txPackets;
        g_totalRxPackets += flow.second.rxPackets;
        totalLostPackets += flow.second.lostPackets;

        double flowDuration = (flow.second.timeLastRxPacket - flow.second.timeFirstTxPacket).GetSeconds();
        if (flow.second.rxPackets > 0 && flowDuration > 0) {
            totalDelay += flow.second.delaySum.GetSeconds();
            totalThroughput += flow.second.rxBytes * 8.0 / flowDuration / 1e6;
            flows_with_packets++;
        }
    }

    double packetDeliveryRatio = (g_totalTxPackets > 0) ? (double)g_totalRxPackets / g_totalTxPackets * 100 : 0;
    double averageDelay = (flows_with_packets > 0) ? totalDelay / flows_with_packets : 0;
    double averageThroughput = (flows_with_packets > 0) ? totalThroughput / flows_with_packets : 0;
    double energyEfficiency = (g_totalEnergyConsumed > 0) ? g_totalRxPackets / g_totalEnergyConsumed : 0;
    double energyPerNode = g_totalEnergyConsumed / nNodes;

    // Print results
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "         ENHANCED MEMOSTP SIMULATION RESULTS         " << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << "NETWORK CONFIGURATION:\n  Nodes: " << nNodes << "\n  Simulation Time: "
              << simulationTime << " s\n  Area: " << area << " m²\n  Servers: " << numServers
              << "\n  Optimization Enabled: " << (enable_optimization ? "Yes" : "No") << std::endl;

    std::cout << "\nTRAFFIC STATISTICS:\n  Packets TX: " << g_totalTxPackets
              << "\n  Packets RX: " << g_totalRxPackets
              << "\n  Packets Lost: " << totalLostPackets
              << "\n  PDR: " << std::fixed << std::setprecision(2) << packetDeliveryRatio << "%"
              << "\n  Avg Delay: " << std::fixed << std::setprecision(4) << averageDelay << " s"
              << "\n  Avg Throughput: " << std::fixed << std::setprecision(3) << averageThroughput << " Mbps" << std::endl;

    std::cout << "\nENERGY STATISTICS:\n  Total Energy: " << std::fixed << std::setprecision(3)
              << g_totalEnergyConsumed << " J\n  Energy/Node: " << energyPerNode
              << " J\n  Energy Efficiency: " << std::fixed << std::setprecision(2)
              << energyEfficiency << " packets/J" << std::endl;

    if (enable_optimization) {
        std::cout << "\nOPTIMIZATION RESULTS:\n  Energy Weight: " << memostp.getEnergyWeight()
                  << "\n  Power Control: " << memostp.getPowerControl()
                  << "\n  Sleep Ratio: " << memostp.getSleepRatio()
                  << "\n  Iterations: " << optimization_iters << std::endl;
    }

    Simulator::Destroy();
    return 0;
} 
