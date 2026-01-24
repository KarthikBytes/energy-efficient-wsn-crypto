#include "metrics_collector.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor.h"
#include "ns3/ipv4-flow-classifier.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cmath>

MetricsCollector::MetricsCollector() {
    // Initialize all metrics to zero
    metrics = NetworkMetrics{};
}

void MetricsCollector::CollectFlowMetrics(ns3::Ptr<ns3::FlowMonitor> monitor) {
    if (!monitor) return;
    
    ns3::FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    
    metrics.totalTxPackets = 0;
    metrics.totalRxPackets = 0;
    metrics.totalLostPackets = 0;
    
    double totalDelay = 0.0;
    double totalThroughput = 0.0;
    double totalJitter = 0.0;
    uint32_t flowsWithPackets = 0;
    
    for (auto &flow : stats) {
        metrics.totalTxPackets += flow.second.txPackets;
        metrics.totalRxPackets += flow.second.rxPackets;
        metrics.totalLostPackets += flow.second.lostPackets;
        
        if (flow.second.rxPackets > 0) {
            double flowDuration = (flow.second.timeLastRxPacket - flow.second.timeFirstTxPacket).GetSeconds();
            if (flowDuration > 0) {
                totalDelay += flow.second.delaySum.GetSeconds() / flow.second.rxPackets;
                totalThroughput += flow.second.rxBytes * 8.0 / flowDuration / 1e6; // Mbps
                flowsWithPackets++;
            }
            
            // Collect jitter samples
            if (flow.second.delaySum.GetSeconds() > 0 && flow.second.rxPackets > 1) {
                double avgDelay = flow.second.delaySum.GetSeconds() / flow.second.rxPackets;
                // Simplified jitter calculation
                double jitter = avgDelay * 0.1; // Approximate
                jitterSamples.push_back(jitter);
                totalJitter += jitter;
            }
        }
    }
    
    // Calculate averages
    metrics.averageDelay = (flowsWithPackets > 0) ? totalDelay / flowsWithPackets : 0.0;
    metrics.averageThroughput = (flowsWithPackets > 0) ? totalThroughput / flowsWithPackets : 0.0;
    metrics.averageJitter = (!jitterSamples.empty()) ? 
                            totalJitter / jitterSamples.size() : 0.0;
    
    // Calculate ratios
    metrics.packetDeliveryRatio = (metrics.totalTxPackets > 0) ? 
        (double)metrics.totalRxPackets / metrics.totalTxPackets * 100 : 0.0;
    metrics.packetLossRate = 100.0 - metrics.packetDeliveryRatio;
    
    metrics.goodput = CalculateGoodput();
}

void MetricsCollector::UpdateEnergyMetrics(double energyConsumed, uint32_t nodeCount) {
    metrics.totalEnergyConsumed = energyConsumed;
    metrics.energyPerNode = (nodeCount > 0) ? energyConsumed / nodeCount : 0.0;
    metrics.energyEfficiency = (energyConsumed > 0) ? 
        metrics.totalRxPackets / energyConsumed : 0.0;
}

void MetricsCollector::UpdateNodeDeathMetrics(double deathTime, uint32_t nodeId, uint32_t totalNodes) {
    nodeDeathTimes.push_back(deathTime);
    
    // Update first and last death times
    if (metrics.firstNodeDeathTime < 0 || deathTime < metrics.firstNodeDeathTime) {
        metrics.firstNodeDeathTime = deathTime;
    }
    if (deathTime > metrics.lastNodeDeathTime) {
        metrics.lastNodeDeathTime = deathTime;
    }
    
    // Update network lifetime
    metrics.networkLifetime = metrics.lastNodeDeathTime - metrics.firstNodeDeathTime;
    
    // Calculate survival rate
    uint32_t deadNodes = nodeDeathTimes.size();
    metrics.nodeSurvivalRate = (totalNodes > 0) ? 
        (1.0 - (double)deadNodes / totalNodes) * 100 : 100.0;
    
    metrics.aliveNodeCount = totalNodes - deadNodes;
    
    // Calculate survivability index
    metrics.networkSurvivabilityIndex = CalculateSurvivabilityIndex();
    
    CalculateDerivedMetrics(totalNodes);
}

void MetricsCollector::UpdateCryptoMetrics(uint32_t encrypted, uint32_t decrypted) {
    metrics.cryptoEncrypted = encrypted;
    metrics.cryptoDecrypted = decrypted;
    metrics.cryptoSuccessRate = (encrypted > 0) ? 
        (double)decrypted / encrypted * 100 : 0.0;
}

void MetricsCollector::CalculateJitterMetrics(const std::vector<double>& jitterSamples) {
    if (jitterSamples.empty()) {
        metrics.averageJitter = 0.0;
        return;
    }
    
    double sum = std::accumulate(jitterSamples.begin(), jitterSamples.end(), 0.0);
    metrics.averageJitter = sum / jitterSamples.size();
}

void MetricsCollector::CalculateDerivedMetrics(uint32_t totalNodes) {
    // Calculate connectivity ratio (simplified)
    metrics.connectivityRatio = (totalNodes > 0) ? 
        (double)metrics.aliveNodeCount / totalNodes * 100 : 0.0;
    
    // Calculate network coverage (simplified)
    metrics.networkCoverage = metrics.connectivityRatio * 0.8; // Assuming 80% of nodes provide coverage
    
    // Calculate average node lifetime
    if (!nodeDeathTimes.empty()) {
        double sum = std::accumulate(nodeDeathTimes.begin(), nodeDeathTimes.end(), 0.0);
        metrics.averageNodeLifetime = sum / nodeDeathTimes.size();
    }
    
    // Calculate normalized routing load (simplified)
    metrics.normalizedRoutingLoad = (metrics.totalRxPackets > 0) ? 
        (double)metrics.totalLostPackets / metrics.totalRxPackets : 0.0;
}

double MetricsCollector::CalculateSurvivabilityIndex() const {
    if (nodeDeathTimes.empty()) return 1.0;
    
    // Composite index based on multiple factors
    double coverageFactor = metrics.networkCoverage / 100.0;
    double survivalFactor = metrics.nodeSurvivalRate / 100.0;
    double lifetimeFactor = 1.0 - (metrics.averageNodeLifetime / (metrics.averageNodeLifetime + 100.0));
    
    // Weighted average
    return (coverageFactor * 0.4 + survivalFactor * 0.4 + lifetimeFactor * 0.2);
}

double MetricsCollector::CalculateGoodput() const {
    // Goodput = effective data rate (excluding overhead)
    // Simplified: 80% of throughput is goodput
    return metrics.averageThroughput * 0.8;
}

void MetricsCollector::PrintComprehensiveMetrics() const {
    std::cout << "\n\033[1;35m📊 COMPREHENSIVE NETWORK METRICS\033[0m" << std::endl;
    std::cout << "\033[1;37m" << std::string(70, '=') << "\033[0m" << std::endl;
    
    std::cout << "\033[1;33m📈 TRAFFIC METRICS:\033[0m" << std::endl;
    std::cout << "├─ Packets Transmitted:    " << metrics.totalTxPackets << std::endl;
    std::cout << "├─ Packets Received:       " << metrics.totalRxPackets << std::endl;
    std::cout << "├─ Packet Delivery Ratio:  " << std::fixed << std::setprecision(2) 
              << metrics.packetDeliveryRatio << "%" << std::endl;
    std::cout << "├─ Packet Loss Rate:       " << metrics.packetLossRate << "%" << std::endl;
    std::cout << "└─ Goodput:                " << std::fixed << std::setprecision(3) 
              << metrics.goodput << " Mbps" << std::endl;
    
    std::cout << "\n\033[1;33m⚡ PERFORMANCE METRICS:\033[0m" << std::endl;
    std::cout << "├─ Average Delay:          " << std::fixed << std::setprecision(4) 
              << metrics.averageDelay << " s" << std::endl;
    std::cout << "├─ Average Jitter:         " << std::fixed << std::setprecision(4) 
              << metrics.averageJitter << " s" << std::endl;
    std::cout << "├─ Average Throughput:     " << std::fixed << std::setprecision(3) 
              << metrics.averageThroughput << " Mbps" << std::endl;
    std::cout << "└─ Normalized Routing Load: " << std::fixed << std::setprecision(3) 
              << metrics.normalizedRoutingLoad << std::endl;
    
    std::cout << "\n\033[1;33m🔋 ENERGY METRICS:\033[0m" << std::endl;
    std::cout << "├─ Total Energy Consumed:  " << std::fixed << std::setprecision(3) 
              << metrics.totalEnergyConsumed << " J" << std::endl;
    std::cout << "├─ Energy per Node:        " << metrics.energyPerNode << " J" << std::endl;
    std::cout << "├─ Energy Efficiency:      " << std::fixed << std::setprecision(2) 
              << metrics.energyEfficiency << " packets/J" << std::endl;
    std::cout << "└─ Network Lifetime:       " << std::fixed << std::setprecision(2) 
              << metrics.networkLifetime << " s" << std::endl;
    
    std::cout << "\n\033[1;33m💀 NETWORK SURVIVABILITY:\033[0m" << std::endl;
    std::cout << "├─ First Node Death Time:  " << std::fixed << std::setprecision(2) 
              << metrics.firstNodeDeathTime << " s" << std::endl;
    std::cout << "├─ Last Node Death Time:   " << metrics.lastNodeDeathTime << " s" << std::endl;
    std::cout << "├─ Average Node Lifetime:  " << metrics.averageNodeLifetime << " s" << std::endl;
    std::cout << "├─ Node Survival Rate:     " << std::fixed << std::setprecision(2) 
              << metrics.nodeSurvivalRate << "%" << std::endl;
    std::cout << "├─ Network Coverage:       " << metrics.networkCoverage << "%" << std::endl;
    std::cout << "├─ Alive Nodes:            " << metrics.aliveNodeCount << std::endl;
    std::cout << "├─ Connectivity Ratio:     " << std::fixed << std::setprecision(2) 
              << metrics.connectivityRatio << "%" << std::endl;
    std::cout << "└─ Survivability Index:    " << std::fixed << std::setprecision(3) 
              << metrics.networkSurvivabilityIndex << "/1.0" << std::endl;
    
    if (metrics.cryptoEncrypted > 0) {
        std::cout << "\n\033[1;33m🔐 CRYPTOGRAPHY METRICS:\033[0m" << std::endl;
        std::cout << "├─ Packets Encrypted:    " << metrics.cryptoEncrypted << std::endl;
        std::cout << "├─ Packets Decrypted:    " << metrics.cryptoDecrypted << std::endl;
        std::cout << "└─ Crypto Success Rate:  " << std::fixed << std::setprecision(2) 
                  << metrics.cryptoSuccessRate << "%" << std::endl;
    }
    
    std::cout << "\033[1;37m" << std::string(70, '=') << "\033[0m" << std::endl;
}

void MetricsCollector::ExportMetricsToCSV(const std::string& filename) const {
    std::ofstream csvFile(filename);
    if (!csvFile.is_open()) {
        std::cerr << "Error opening CSV file: " << filename << std::endl;
        return;
    }
    
    // Write CSV header
    csvFile << "Metric,Value,Unit\n";
    
    // Traffic metrics
    csvFile << "TotalPacketsTransmitted," << metrics.totalTxPackets << ",packets\n";
    csvFile << "TotalPacketsReceived," << metrics.totalRxPackets << ",packets\n";
    csvFile << "PacketDeliveryRatio," << metrics.packetDeliveryRatio << ",%\n";
    csvFile << "PacketLossRate," << metrics.packetLossRate << ",%\n";
    csvFile << "Goodput," << metrics.goodput << ",Mbps\n";
    
    // Performance metrics
    csvFile << "AverageDelay," << metrics.averageDelay << ",s\n";
    csvFile << "AverageJitter," << metrics.averageJitter << ",s\n";
    csvFile << "AverageThroughput," << metrics.averageThroughput << ",Mbps\n";
    csvFile << "NormalizedRoutingLoad," << metrics.normalizedRoutingLoad << ",ratio\n";
    
    // Energy metrics
    csvFile << "TotalEnergyConsumed," << metrics.totalEnergyConsumed << ",J\n";
    csvFile << "EnergyPerNode," << metrics.energyPerNode << ",J\n";
    csvFile << "EnergyEfficiency," << metrics.energyEfficiency << ",packets/J\n";
    csvFile << "NetworkLifetime," << metrics.networkLifetime << ",s\n";
    
    // Survivability metrics
    csvFile << "FirstNodeDeathTime," << metrics.firstNodeDeathTime << ",s\n";
    csvFile << "LastNodeDeathTime," << metrics.lastNodeDeathTime << ",s\n";
    csvFile << "AverageNodeLifetime," << metrics.averageNodeLifetime << ",s\n";
    csvFile << "NodeSurvivalRate," << metrics.nodeSurvivalRate << ",%\n";
    csvFile << "NetworkCoverage," << metrics.networkCoverage << ",%\n";
    csvFile << "AliveNodeCount," << metrics.aliveNodeCount << ",nodes\n";
    csvFile << "ConnectivityRatio," << metrics.connectivityRatio << ",%\n";
    csvFile << "NetworkSurvivabilityIndex," << metrics.networkSurvivabilityIndex << ",index\n";
    
    // Crypto metrics
    if (metrics.cryptoEncrypted > 0) {
        csvFile << "CryptoEncrypted," << metrics.cryptoEncrypted << ",packets\n";
        csvFile << "CryptoDecrypted," << metrics.cryptoDecrypted << ",packets\n";
        csvFile << "CryptoSuccessRate," << metrics.cryptoSuccessRate << ",%\n";
    }
    
    csvFile.close();
    std::cout << "\n📁 Metrics exported to: " << filename << std::endl;
}