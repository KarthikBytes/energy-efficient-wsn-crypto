#ifndef METRICS_COLLECTOR_H
#define METRICS_COLLECTOR_H

#include <map>
#include <vector>
#include <string>
#include <fstream>

namespace ns3 {
    class FlowMonitor;
}

class MetricsCollector {
public:
    struct NetworkMetrics {
        // Traffic metrics
        uint32_t totalTxPackets;
        uint32_t totalRxPackets;
        uint32_t totalLostPackets;
        
        // Performance metrics
        double packetDeliveryRatio;
        double averageDelay;
        double averageThroughput;
        double averageJitter;
        
        // Energy metrics
        double totalEnergyConsumed;
        double energyEfficiency;
        double energyPerNode;
        
        // Network lifetime metrics
        double networkLifetime;
        double firstNodeDeathTime;
        double lastNodeDeathTime;
        double averageNodeLifetime;
        uint32_t aliveNodeCount;
        double networkCoverage;
        
        // Survivability metrics
        double networkSurvivabilityIndex;
        double nodeSurvivalRate;
        double connectivityRatio;
        
        // QoS metrics
        double packetLossRate;
        double goodput;
        double normalizedRoutingLoad;
        
        // Crypto metrics
        uint32_t cryptoEncrypted;
        uint32_t cryptoDecrypted;
        double cryptoSuccessRate;
    };
    
    MetricsCollector();
    
    void CollectFlowMetrics(ns3::Ptr<ns3::FlowMonitor> monitor);
    void UpdateEnergyMetrics(double energyConsumed, uint32_t nodeCount);
    void UpdateNodeDeathMetrics(double deathTime, uint32_t nodeId, uint32_t totalNodes);
    void UpdateCryptoMetrics(uint32_t encrypted, uint32_t decrypted);
    void CalculateJitterMetrics(const std::vector<double>& jitterSamples);
    
    NetworkMetrics GetMetrics() const { return metrics; }
    void PrintComprehensiveMetrics() const;
    void ExportMetricsToCSV(const std::string& filename) const;
    
private:
    NetworkMetrics metrics;
    std::vector<double> delaySamples;
    std::vector<double> jitterSamples;
    std::vector<double> nodeDeathTimes;
    
    void CalculateDerivedMetrics(uint32_t totalNodes);
    double CalculateSurvivabilityIndex() const;
    double CalculateGoodput() const;
};

#endif // METRICS_COLLECTOR_H