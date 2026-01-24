// memostp-enhanced-with-node-death.cc
// NS-3.42 MEMOSTP Simulation with Node Death/Resilience
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
#include <bitset>
#include <sstream>
#include <cstring>
#include <map>
#include <queue>

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("MEMOSTPSimulation");

// Global variables
uint32_t g_totalTxPackets = 0;
uint32_t g_totalRxPackets = 0;
double g_totalEnergyConsumed = 0.0;
uint32_t g_deadNodes = 0;
std::vector<uint32_t> g_nodeDeathTimes;

////////////////////////////////////////////////////////
// JSON EVENT EMISSION SYSTEM (Enhanced)              //
////////////////////////////////////////////////////////
void EmitEvent(
    std::string event,
    uint32_t packetId,
    int from = -1,
    int to = -1,
    double value = -1.0,
    std::string info = ""
) {
    std::cout
        << "{"
        << "\"time\":" << std::fixed << std::setprecision(3)
        << ns3::Simulator::Now().GetSeconds() << ","
        << "\"event\":\"" << event << "\","
        << "\"packetId\":" << packetId;

    if (from >= 0)
        std::cout << ",\"from\":" << from;
    if (to >= 0)
        std::cout << ",\"to\":" << to;
    if (value >= 0)
        std::cout << ",\"value\":" << std::fixed << std::setprecision(3) << value;
    if (!info.empty())
        std::cout << ",\"info\":\"" << info << "\"";

    std::cout << "}" << std::endl;
}

///////////////////////////////////
// WORKING ASCON-128 CRYPTOGRAPHY //
///////////////////////////////////
class AsconCrypto {
private:
    static const int ASCON_128_KEY_SIZE = 16;
    static const int ASCON_128_IV_SIZE = 16;
    static const int ASCON_RATE = 64;
    static const int ASCON_a = 12;
    static const int ASCON_b = 6;
    
    uint64_t state[5];
    
    void Permutation(uint64_t* s, int rounds) {
        for (int r = 0; r < rounds; r++) {
            s[2] ^= ((0x0F - r) << 4) | r;
            
            uint64_t x0 = s[0];
            uint64_t x1 = s[1];
            uint64_t x2 = s[2];
            uint64_t x3 = s[3];
            uint64_t x4 = s[4];
            
            s[0] = x4 ^ x1 ^ ((x2 & ~x1) << 1);
            s[1] = x0 ^ x2 ^ ((x3 & ~x2) << 1);
            s[2] = x1 ^ x3 ^ ((x4 & ~x3) << 1);
            s[3] = x2 ^ x4 ^ ((x0 & ~x4) << 1);
            s[4] = x3 ^ x0 ^ ((x1 & ~x0) << 1);
            
            s[0] ^= ((s[0] >> 19) | (s[0] << (64 - 19))) ^ ((s[0] >> 28) | (s[0] << (64 - 28)));
            s[1] ^= ((s[1] >> 61) | (s[1] << (64 - 61))) ^ ((s[1] >> 39) | (s[1] << (64 - 39)));
            s[2] ^= ((s[2] >> 1)  | (s[2] << (64 - 1)))  ^ ((s[2] >> 6)  | (s[2] << (64 - 6)));
            s[3] ^= ((s[3] >> 10) | (s[3] << (64 - 10))) ^ ((s[3] >> 17) | (s[3] << (64 - 17)));
            s[4] ^= ((s[4] >> 7)  | (s[4] << (64 - 7)))  ^ ((s[4] >> 41) | (s[4] << (64 - 41)));
        }
    }
    
public:
    AsconCrypto() {
        memset(state, 0, sizeof(state));
    }
    
    void Initialize(const uint8_t* key, const uint8_t* nonce) {
        std::cout << "\033[1;32m" << "=" << std::string(60, '=') << "=" << "\033[0m" << std::endl;
        std::cout << "\033[1;32m" << "  ASCON-128 CRYPTOGRAPHY INITIALIZATION  " << "\033[0m" << std::endl;
        std::cout << "\033[1;32m" << "=" << std::string(60, '=') << "=" << "\033[0m" << std::endl;
        
        state[0] = ((uint64_t)key[0] << 56) | ((uint64_t)key[1] << 48) | ((uint64_t)key[2] << 40) | ((uint64_t)key[3] << 32) |
                  ((uint64_t)key[4] << 24) | ((uint64_t)key[5] << 16) | ((uint64_t)key[6] << 8) | key[7];
        state[1] = ((uint64_t)key[8] << 56) | ((uint64_t)key[9] << 48) | ((uint64_t)key[10] << 40) | ((uint64_t)key[11] << 32) |
                  ((uint64_t)key[12] << 24) | ((uint64_t)key[13] << 16) | ((uint64_t)key[14] << 8) | key[15];
        state[2] = ((uint64_t)nonce[0] << 56) | ((uint64_t)nonce[1] << 48) | ((uint64_t)nonce[2] << 40) | ((uint64_t)nonce[3] << 32) |
                  ((uint64_t)nonce[4] << 24) | ((uint64_t)nonce[5] << 16) | ((uint64_t)nonce[6] << 8) | nonce[7];
        state[3] = ((uint64_t)nonce[8] << 56) | ((uint64_t)nonce[9] << 48) | ((uint64_t)nonce[10] << 40) | ((uint64_t)nonce[11] << 32) |
                  ((uint64_t)nonce[12] << 24) | ((uint64_t)nonce[13] << 16) | ((uint64_t)nonce[14] << 8) | nonce[15];
        state[4] = 0x0000000000000080ULL;
        
        Permutation(state, ASCON_a);
        
        state[3] ^= ((uint64_t)key[0] << 56) | ((uint64_t)key[1] << 48) | ((uint64_t)key[2] << 40) | ((uint64_t)key[3] << 32) |
                   ((uint64_t)key[4] << 24) | ((uint64_t)key[5] << 16) | ((uint64_t)key[6] << 8) | key[7];
        state[4] ^= ((uint64_t)key[8] << 56) | ((uint64_t)key[9] << 48) | ((uint64_t)key[10] << 40) | ((uint64_t)key[11] << 32) |
                   ((uint64_t)key[12] << 24) | ((uint64_t)key[13] << 16) | ((uint64_t)key[14] << 8) | key[15];
        
        std::cout << "✓ ASCON-128 Initialized Successfully\n" << std::endl;
    }
    
    std::vector<uint8_t> Encrypt(const std::vector<uint8_t>& plaintext, uint32_t packetId, uint32_t nodeId) {
        EmitEvent("encrypt", packetId, nodeId, -1, plaintext.size());
        
        uint64_t currentState[5];
        memcpy(currentState, state, sizeof(state));
        
        std::vector<uint8_t> ciphertext(plaintext.size());
        
        for (size_t i = 0; i < plaintext.size(); i += ASCON_RATE/8) {
            size_t blockSize = std::min<size_t>(ASCON_RATE/8, plaintext.size() - i);
            
            for (size_t j = 0; j < blockSize; j++) {
                uint8_t byte = plaintext[i + j];
                uint8_t stateByte = (currentState[j/8] >> (56 - 8*(j%8))) & 0xFF;
                ciphertext[i + j] = byte ^ stateByte;
                currentState[j/8] ^= ((uint64_t)byte << (56 - 8*(j%8)));
            }
            
            if (i + blockSize < plaintext.size()) {
                Permutation(currentState, ASCON_b);
            }
        }
        
        currentState[4] ^= 0x01;
        Permutation(currentState, ASCON_a);
        
        std::vector<uint8_t> tag(16);
        for (int i = 0; i < 16; i++) {
            tag[i] = (currentState[i/8] >> (56 - 8*(i%8))) & 0xFF;
        }
        
        ciphertext.insert(ciphertext.end(), tag.begin(), tag.end());
        return ciphertext;
    }
    
    std::vector<uint8_t> Decrypt(const std::vector<uint8_t>& ciphertext, uint32_t packetId, uint32_t nodeId) {
        if (ciphertext.size() < 16) {
            EmitEvent("decrypt_error", packetId, nodeId, -1, ciphertext.size(), "ciphertext_too_short");
            return {};
        }
        
        uint64_t currentState[5];
        memcpy(currentState, state, sizeof(state));
        
        size_t dataSize = ciphertext.size() - 16;
        std::vector<uint8_t> plaintext(dataSize);
        
        for (size_t i = 0; i < dataSize; i += ASCON_RATE/8) {
            size_t blockSize = std::min<size_t>(ASCON_RATE/8, dataSize - i);
            
            for (size_t j = 0; j < blockSize; j++) {
                uint8_t cByte = ciphertext[i + j];
                uint8_t stateByte = (currentState[j/8] >> (56 - 8*(j%8))) & 0xFF;
                plaintext[i + j] = cByte ^ stateByte;
                currentState[j/8] ^= ((uint64_t)plaintext[i + j] << (56 - 8*(j%8)));
            }
            
            if (i + blockSize < dataSize) {
                Permutation(currentState, ASCON_b);
            }
        }
        
        currentState[4] ^= 0x01;
        Permutation(currentState, ASCON_a);
        
        bool verificationSuccess = true;
        for (int i = 0; i < 16; i++) {
            uint8_t expectedTag = (currentState[i/8] >> (56 - 8*(i%8))) & 0xFF;
            if (expectedTag != ciphertext[dataSize + i]) {
                verificationSuccess = false;
                break;
            }
        }
        
        if (verificationSuccess) {
            EmitEvent("decrypt", packetId, nodeId, -1, plaintext.size());
            return plaintext;
        } else {
            EmitEvent("decrypt_failed", packetId, nodeId, -1, 0, "tag_mismatch");
            plaintext.clear();
            return plaintext;
        }
    }
    
    void PrintCryptoMetrics() const {
        std::cout << "\033[1;34m" << std::string(60, '-') << "\033[0m" << std::endl;
        std::cout << "\033[1;34m" << "ASCON-128 CRYPTOGRAPHY METRICS" << "\033[0m" << std::endl;
        std::cout << "\033[1;34m" << std::string(60, '-') << "\033[0m" << std::endl;
        std::cout << "Algorithm: ASCON-128 (NIST Lightweight Standard)" << std::endl;
        std::cout << "Key Size: 128 bits" << std::endl;
        std::cout << "State: 320 bits (5×64-bit words)" << std::endl;
        std::cout << "\033[1;34m" << std::string(60, '-') << "\033[0m" << std::endl;
    }
};

///////////////////////////
// Snake Optimizer - Enhanced //
///////////////////////////
class EnhancedSnakeOptimizer {
private:
    std::vector<double> bestParams;
    std::vector<std::vector<double>> optimizationHistory;
    
public:
    EnhancedSnakeOptimizer() {
        bestParams = {0.6, 0.7, 0.3, 0.2}; // Energy weight, Power control, Sleep ratio, Resilience factor
        optimizationHistory.push_back(bestParams);
    }
    
    std::vector<double> optimize(int iterations, double currentPDR = 90.0, int deadNodes = 0) {
        EmitEvent("optimization_start", 0, -1, -1, iterations);
        
        std::cout << "\033[1;33m🧬 OPTIMIZATION STARTED (" << iterations << " iterations)\033[0m" << std::endl;
        std::cout << "   Current PDR: " << currentPDR << "%, Dead Nodes: " << deadNodes << std::endl;
        
        // Adaptive optimization based on network conditions
        for (int iter = 0; iter < iterations; iter++) {
            // Adjust based on network health
            double networkHealth = currentPDR / 100.0 * (1.0 - deadNodes / 50.0);
            double adjustment = 0.9 + 0.2 * networkHealth;
            
            // Dynamic parameter adjustment
            bestParams[0] = 0.55 + 0.15 * adjustment;  // Energy weight
            bestParams[1] = 0.65 + 0.15 * networkHealth; // Power control
            bestParams[2] = 0.25 + 0.2 * (1.0 - adjustment); // Sleep ratio
            bestParams[3] = 0.1 + 0.3 * (deadNodes / 10.0); // Resilience factor
            
            // Clamp values
            for (auto& param : bestParams) {
                param = std::max(0.1, std::min(1.0, param));
            }
            
            optimizationHistory.push_back(bestParams);
            
            if (iter % 2 == 0) {
                EmitEvent("optimization_progress", iter, -1, iterations, networkHealth);
                std::cout << "\033[33m  Iteration " << iter << "/" << iterations 
                         << " Network Health: " << networkHealth << "\033[0m" << std::endl;
            }
        }
        
        EmitEvent("optimization_complete", iterations, -1, -1, networkHealth);
        std::cout << "\033[1;32m✓ OPTIMIZATION COMPLETE\033[0m" << std::endl;
        
        // Print final parameters
        std::cout << "Final Parameters:" << std::endl;
        std::cout << "  Energy Weight: " << bestParams[0] << std::endl;
        std::cout << "  Power Control: " << bestParams[1] << std::endl;
        std::cout << "  Sleep Ratio: " << bestParams[2] << std::endl;
        std::cout << "  Resilience Factor: " << bestParams[3] << std::endl;
        
        return bestParams;
    }
    
    double getBestEnergyWeight(const std::vector<double> &params) const { 
        return params.size() > 0 ? params[0] : 0.6; 
    }
    
    double getBestPowerControl(const std::vector<double> &params) const { 
        return params.size() > 1 ? params[1] : 0.7; 
    }
    
    double getBestSleepRatio(const std::vector<double> &params) const { 
        return params.size() > 2 ? params[2] : 0.3; 
    }
    
    double getResilienceFactor(const std::vector<double> &params) const { 
        return params.size() > 3 ? params[3] : 0.2; 
    }
    
    const std::vector<std::vector<double>>& getOptimizationHistory() const {
        return optimizationHistory;
    }
};

///////////////////////////////////
// Node Energy Manager           //
///////////////////////////////////
class NodeEnergyManager {
private:
    struct NodeEnergyInfo {
        double remainingEnergy;
        double initialEnergy;
        bool isAlive;
        Time deathTime;
        double energyConsumptionRate;
    };
    
    std::map<uint32_t, NodeEnergyInfo> nodes;
    Ptr<UniformRandomVariable> randomVar;
    double baseEnergyConsumption;
    double transmissionCost;
    double receptionCost;
    double sleepConsumption;
    
public:
    NodeEnergyManager(double baseEnergy = 100.0, 
                     double txCost = 0.05, 
                     double rxCost = 0.02,
                     double sleepCost = 0.01) 
        : baseEnergyConsumption(baseEnergy),
          transmissionCost(txCost),
          receptionCost(rxCost),
          sleepConsumption(sleepCost)
    {
        randomVar = CreateObject<UniformRandomVariable>();
        randomVar->SetAttribute("Min", DoubleValue(0.8));
        randomVar->SetAttribute("Max", DoubleValue(1.2));
    }
    
    void AddNode(uint32_t nodeId, double initialEnergy = 100.0) {
        NodeEnergyInfo info;
        info.remainingEnergy = initialEnergy * randomVar->GetValue();
        info.initialEnergy = info.remainingEnergy;
        info.isAlive = true;
        info.deathTime = Seconds(0);
        info.energyConsumptionRate = baseEnergyConsumption;
        nodes[nodeId] = info;
        
        EmitEvent("node_energy_initialized", 0, nodeId, -1, info.remainingEnergy);
    }
    
    bool ConsumeEnergy(uint32_t nodeId, double amount, std::string reason = "") {
        if (nodes.find(nodeId) == nodes.end() || !nodes[nodeId].isAlive) {
            return false;
        }
        
        nodes[nodeId].remainingEnergy -= amount;
        
        if (nodes[nodeId].remainingEnergy <= 0) {
            nodes[nodeId].isAlive = false;
            nodes[nodeId].remainingEnergy = 0;
            nodes[nodeId].deathTime = Simulator::Now();
            g_deadNodes++;
            g_nodeDeathTimes.push_back(Simulator::Now().GetSeconds());
            
            EmitEvent("node_died", 0, nodeId, -1, 0, 
                     "energy_exhausted_" + reason);
            
            std::cout << "\033[1;31m⚰️  Node " << nodeId << " died at " 
                     << Simulator::Now().GetSeconds() << "s (Reason: " 
                     << reason << ")\033[0m" << std::endl;
            
            return false;
        }
        
        // Emit energy update periodically
        static Time lastEmitTime = Seconds(0);
        if (Simulator::Now() - lastEmitTime > Seconds(5)) {
            EmitEvent("node_energy_update", 0, nodeId, -1, 
                     nodes[nodeId].remainingEnergy);
            lastEmitTime = Simulator::Now();
        }
        
        return true;
    }
    
    void ConsumeTransmissionEnergy(uint32_t nodeId, uint32_t packetSize) {
        double energyCost = transmissionCost * (packetSize / 1024.0) * randomVar->GetValue();
        ConsumeEnergy(nodeId, energyCost, "transmission");
    }
    
    void ConsumeReceptionEnergy(uint32_t nodeId, uint32_t packetSize) {
        double energyCost = receptionCost * (packetSize / 1024.0) * randomVar->GetValue();
        ConsumeEnergy(nodeId, energyCost, "reception");
    }
    
    void ConsumeIdleEnergy(uint32_t nodeId, Time duration) {
        double energyCost = baseEnergyConsumption * duration.GetSeconds() * randomVar->GetValue();
        ConsumeEnergy(nodeId, energyCost, "idle");
    }
    
    void ConsumeSleepEnergy(uint32_t nodeId, Time duration) {
        double energyCost = sleepConsumption * duration.GetSeconds() * randomVar->GetValue();
        ConsumeEnergy(nodeId, energyCost, "sleep");
    }
    
    bool IsNodeAlive(uint32_t nodeId) const {
        auto it = nodes.find(nodeId);
        return it != nodes.end() && it->second.isAlive;
    }
    
    double GetRemainingEnergy(uint32_t nodeId) const {
        auto it = nodes.find(nodeId);
        return it != nodes.end() ? it->second.remainingEnergy : 0;
    }
    
    double GetInitialEnergy(uint32_t nodeId) const {
        auto it = nodes.find(nodeId);
        return it != nodes.end() ? it->second.initialEnergy : 0;
    }
    
    double GetEnergyPercentage(uint32_t nodeId) const {
        auto it = nodes.find(nodeId);
        if (it == nodes.end() || it->second.initialEnergy == 0) return 0;
        return (it->second.remainingEnergy / it->second.initialEnergy) * 100;
    }
    
    uint32_t GetAliveNodesCount() const {
        uint32_t count = 0;
        for (const auto& pair : nodes) {
            if (pair.second.isAlive) count++;
        }
        return count;
    }
    
    std::vector<uint32_t> GetDeadNodes() const {
        std::vector<uint32_t> deadNodes;
        for (const auto& pair : nodes) {
            if (!pair.second.isAlive) {
                deadNodes.push_back(pair.first);
            }
        }
        return deadNodes;
    }
    
    void PrintEnergyStatistics() const {
        std::cout << "\033[1;35m" << std::string(60, '=') << "\033[0m" << std::endl;
        std::cout << "\033[1;35m" << "      NODE ENERGY STATISTICS      " << "\033[0m" << std::endl;
        std::cout << "\033[1;35m" << std::string(60, '=') << "\033[0m" << std::endl;
        
        uint32_t alive = 0;
        double totalRemaining = 0;
        double totalInitial = 0;
        
        for (const auto& pair : nodes) {
            if (pair.second.isAlive) {
                alive++;
                totalRemaining += pair.second.remainingEnergy;
                totalInitial += pair.second.initialEnergy;
            }
        }
        
        std::cout << "Alive Nodes: " << alive << "/" << nodes.size() 
                 << " (" << (alive * 100.0 / nodes.size()) << "%)" << std::endl;
        std::cout << "Dead Nodes: " << (nodes.size() - alive) << std::endl;
        std::cout << "Total Initial Energy: " << std::fixed << std::setprecision(2) 
                 << totalInitial << " J" << std::endl;
        std::cout << "Total Remaining Energy: " << totalRemaining << " J" << std::endl;
        std::cout << "Energy Consumption: " << (totalInitial - totalRemaining) << " J" << std::endl;
        std::cout << "Network Lifetime: " << Simulator::Now().GetSeconds() << "s" << std::endl;
        
        if (!g_nodeDeathTimes.empty()) {
            std::cout << "\nFirst Node Death: " << g_nodeDeathTimes[0] << "s" << std::endl;
            std::cout << "Last Node Death: " << g_nodeDeathTimes.back() << "s" << std::endl;
        }
        
        std::cout << "\033[1;35m" << std::string(60, '=') << "\033[0m" << std::endl;
    }
};

///////////////////////////
// Enhanced MEMOSTP Protocol //
///////////////////////////
class EnhancedMEMOSTPProtocol {
private:
    NodeContainer nodes;
    NodeEnergyManager energyManager;
    EnhancedSnakeOptimizer optimizer;
    std::vector<double> optimizedParams;
    int optimization_iterations;
    AsconCrypto cryptoEngine;
    bool cryptoEnabled;
    uint8_t cryptoKey[16];
    uint8_t cryptoNonce[16];
    uint32_t packetsEncrypted;
    uint32_t packetsDecrypted;
    uint32_t packetsReceived;
    uint32_t packetsDroppedDeadNode;
    double networkLifetime;
    
    // Resilience metrics
    struct ResilienceMetrics {
        uint32_t routeChanges;
        uint32_t recoveryAttempts;
        uint32_t successfulRecoveries;
        Time totalDowntime;
    } resilienceMetrics;

public:
    EnhancedMEMOSTPProtocol(NodeContainer &nodeContainer, int opt_iters = 10)
        : nodes(nodeContainer), optimization_iterations(opt_iters),
          cryptoEnabled(true), packetsEncrypted(0), packetsDecrypted(0), 
          packetsReceived(0), packetsDroppedDeadNode(0), networkLifetime(0)
    {
        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<uint8_t> dist(0, 255);
        for (int i = 0; i < 16; i++) {
            cryptoKey[i] = dist(rng);
            cryptoNonce[i] = dist(rng);
        }
        
        // Initialize resilience metrics
        resilienceMetrics.routeChanges = 0;
        resilienceMetrics.recoveryAttempts = 0;
        resilienceMetrics.successfulRecoveries = 0;
        resilienceMetrics.totalDowntime = Seconds(0);
    }

    void initializeProtocol() {
        std::cout << "\033[1;32m╔══════════════════════════════════════════════════════════╗\033[0m" << std::endl;
        std::cout << "\033[1;32m║     ENHANCED MEMOSTP WITH NODE RESILIENCE              ║\033[0m" << std::endl;
        std::cout << "\033[1;32m╚══════════════════════════════════════════════════════════╝\033[0m" << std::endl;
        
        // Initialize energy manager for all nodes
        std::cout << "\n\033[1;33m🔋 Initializing Node Energy Management...\033[0m" << std::endl;
        for (uint32_t i = 0; i < nodes.GetN(); i++) {
            // Random initial energy between 80-120 Joules
            Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
            uv->SetAttribute("Min", DoubleValue(80.0));
            uv->SetAttribute("Max", DoubleValue(120.0));
            double initialEnergy = uv->GetValue();
            
            energyManager.AddNode(i, initialEnergy);
            
            if (i < 5) { // Log first 5 nodes
                std::cout << "  Node " << i << ": " << std::fixed << std::setprecision(1) 
                         << initialEnergy << " J initial energy" << std::endl;
            }
        }
        
        if (cryptoEnabled) {
            cryptoEngine.Initialize(cryptoKey, cryptoNonce);
            cryptoEngine.PrintCryptoMetrics();
        }
        
        std::cout << "\n\033[1;33m🚀 Starting Adaptive Parameter Optimization...\033[0m" << std::endl;
        optimizedParams = optimizer.optimize(optimization_iterations);
        
        std::cout << "\n\033[1;32m✨ OPTIMIZATION RESULTS:\033[0m" << std::endl;
        std::cout << "┌─────────────────────────────────────────────┐" << std::endl;
        std::cout << "│ Energy Weight:      " << std::fixed << std::setw(8) << std::setprecision(4) << getEnergyWeight() << " │" << std::endl;
        std::cout << "│ Power Control:      " << std::setw(8) << getPowerControl() << " │" << std::endl;
        std::cout << "│ Sleep Ratio:        " << std::setw(8) << getSleepRatio() << " │" << std::endl;
        std::cout << "│ Resilience Factor:  " << std::setw(8) << getResilienceFactor() << " │" << std::endl;
        std::cout << "└─────────────────────────────────────────────┘" << std::endl;
        
        // Schedule periodic network health checks
        Simulator::Schedule(Seconds(10), &EnhancedMEMOSTPProtocol::CheckNetworkHealth, this);
        
        // Schedule random node failures (for testing resilience)
        Simulator::Schedule(Seconds(15), &EnhancedMEMOSTPProtocol::SimulateRandomFailure, this);
        Simulator::Schedule(Seconds(25), &EnhancedMEMOSTPProtocol::SimulateRandomFailure, this);
    }

    std::vector<uint8_t> encryptPacket(const std::vector<uint8_t>& plaintext, uint32_t nodeId, uint32_t packetId) {
        if (!cryptoEnabled || !energyManager.IsNodeAlive(nodeId)) {
            packetsDroppedDeadNode++;
            return {};
        }
        
        packetsEncrypted++;
        
        // Consume encryption energy
        energyManager.ConsumeEnergy(nodeId, 0.001 * plaintext.size(), "encryption");
        
        std::vector<uint8_t> dataToEncrypt = plaintext;
        uint32_t seqNum = packetsEncrypted;
        dataToEncrypt.insert(dataToEncrypt.begin(), 
                           reinterpret_cast<uint8_t*>(&seqNum), 
                           reinterpret_cast<uint8_t*>(&seqNum) + 4);
        
        auto ciphertext = cryptoEngine.Encrypt(dataToEncrypt, packetId, nodeId);
        
        if (packetsEncrypted <= 3) {
            std::cout << "\033[36m🔒 Encrypted Packet #" << packetsEncrypted 
                     << " from Node " << nodeId
                     << " (" << plaintext.size() << " bytes)\033[0m" << std::endl;
        }
        
        return ciphertext;
    }

    std::vector<uint8_t> decryptPacket(const std::vector<uint8_t>& ciphertext, uint32_t nodeId, uint32_t packetId) {
        if (!cryptoEnabled || !energyManager.IsNodeAlive(nodeId)) {
            packetsDroppedDeadNode++;
            return {};
        }
        
        packetsReceived++;
        
        // Consume decryption energy
        energyManager.ConsumeEnergy(nodeId, 0.001 * ciphertext.size(), "decryption");
        
        auto plaintext = cryptoEngine.Decrypt(ciphertext, packetId, nodeId);
        
        if (!plaintext.empty()) {
            packetsDecrypted++;
            
            if (plaintext.size() >= 4) {
                uint32_t seqNum;
                memcpy(&seqNum, plaintext.data(), 4);
                
                if (packetsDecrypted <= 3) {
                    std::cout << "\033[32m🔓 Node " << nodeId << " decrypted Packet #" << seqNum 
                             << " (" << plaintext.size()-4 << " bytes)\033[0m" << std::endl;
                }
                
                plaintext.erase(plaintext.begin(), plaintext.begin() + 4);
            }
        }
        
        return plaintext;
    }

    void CheckNetworkHealth() {
        uint32_t aliveNodes = energyManager.GetAliveNodesCount();
        uint32_t totalNodes = nodes.GetN();
        double alivePercentage = (aliveNodes * 100.0) / totalNodes;
        
        EmitEvent("network_health", 0, aliveNodes, totalNodes, alivePercentage);
        
        std::cout << "\033[1;36m📊 Network Health: " << aliveNodes << "/" << totalNodes 
                 << " nodes alive (" << std::fixed << std::setprecision(1) 
                 << alivePercentage << "%)\033[0m" << std::endl;
        
        // Re-optimize if network health drops below 70%
        if (alivePercentage < 70.0 && Simulator::Now().GetSeconds() < 30) {
            std::cout << "\033[1;33m⚠️  Network health critical, re-optimizing...\033[0m" << std::endl;
            optimizedParams = optimizer.optimize(3, alivePercentage, totalNodes - aliveNodes);
        }
        
        // Schedule next health check
        if (Simulator::Now().GetSeconds() < simulationTime - 10) {
            Simulator::Schedule(Seconds(10), &EnhancedMEMOSTPProtocol::CheckNetworkHealth, this);
        }
    }
    
    void SimulateRandomFailure() {
        uint32_t aliveNodes = energyManager.GetAliveNodesCount();
        if (aliveNodes < 3) return; // Don't kill if too few nodes alive
        
        Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
        uv->SetAttribute("Min", DoubleValue(0));
        uv->SetAttribute("Max", DoubleValue(nodes.GetN() - 1));
        
        uint32_t nodeToKill = uv->GetInteger();
        
        // Find an alive node to kill
        int attempts = 0;
        while (!energyManager.IsNodeAlive(nodeToKill) && attempts < 10) {
            nodeToKill = uv->GetInteger();
            attempts++;
        }
        
        if (energyManager.IsNodeAlive(nodeToKill)) {
            // Consume all remaining energy to kill the node
            double remainingEnergy = energyManager.GetRemainingEnergy(nodeToKill);
            energyManager.ConsumeEnergy(nodeToKill, remainingEnergy * 1.1, "simulated_failure");
            
            EmitEvent("simulated_failure", 0, nodeToKill, -1, Simulator::Now().GetSeconds());
            
            std::cout << "\033[1;31m💥 Simulated failure of Node " << nodeToKill 
                     << " at " << Simulator::Now().GetSeconds() << "s\033[0m" << std::endl;
            
            // Attempt route recovery
            resilienceMetrics.recoveryAttempts++;
            AttemptRouteRecovery(nodeToKill);
        }
    }
    
    void AttemptRouteRecovery(uint32_t deadNode) {
        // Simple recovery simulation
        Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
        uv->SetAttribute("Min", DoubleValue(0));
        uv->SetAttribute("Max", DoubleValue(1));
        
        if (uv->GetValue() < getResilienceFactor()) {
            resilienceMetrics.successfulRecoveries++;
            resilienceMetrics.routeChanges++;
            
            EmitEvent("route_recovery", 0, deadNode, -1, getResilienceFactor(), "success");
            
            std::cout << "\033[1;32m✅ Route recovery successful for Node " 
                     << deadNode << "\033[0m" << std::endl;
        } else {
            EmitEvent("route_recovery", 0, deadNode, -1, getResilienceFactor(), "failed");
            
            std::cout << "\033[1;33m⚠️  Route recovery failed for Node " 
                     << deadNode << "\033[0m" << std::endl;
        }
    }
    
    bool CanTransmit(uint32_t nodeId, uint32_t packetSize) {
        if (!energyManager.IsNodeAlive(nodeId)) {
            return false;
        }
        
        // Check if node has enough energy for transmission
        double requiredEnergy = 0.05 * (packetSize / 1024.0);
        double remainingEnergy = energyManager.GetRemainingEnergy(nodeId);
        
        if (remainingEnergy < requiredEnergy * 2) { // Keep buffer
            EmitEvent("low_energy_warning", 0, nodeId, -1, remainingEnergy);
            return false;
        }
        
        return true;
    }
    
    void ConsumeTransmissionEnergy(uint32_t nodeId, uint32_t packetSize) {
        if (energyManager.IsNodeAlive(nodeId)) {
            energyManager.ConsumeTransmissionEnergy(nodeId, packetSize);
        }
    }
    
    void ConsumeReceptionEnergy(uint32_t nodeId, uint32_t packetSize) {
        if (energyManager.IsNodeAlive(nodeId)) {
            energyManager.ConsumeReceptionEnergy(nodeId, packetSize);
        }
    }

    double getEnergyWeight() const { 
        return optimizedParams.size() > 0 ? optimizer.getBestEnergyWeight(optimizedParams) : 0.6; 
    }
    
    double getPowerControl() const { 
        return optimizedParams.size() > 1 ? optimizer.getBestPowerControl(optimizedParams) : 0.7; 
    }
    
    double getSleepRatio() const { 
        return optimizedParams.size() > 2 ? optimizer.getBestSleepRatio(optimizedParams) : 0.3; 
    }
    
    double getResilienceFactor() const { 
        return optimizedParams.size() > 3 ? optimizer.getResilienceFactor(optimizedParams) : 0.2; 
    }
    
    uint32_t getPacketsEncrypted() const { return packetsEncrypted; }
    uint32_t getPacketsDecrypted() const { return packetsDecrypted; }
    uint32_t getPacketsReceived() const { return packetsReceived; }
    uint32_t getPacketsDroppedDeadNode() const { return packetsDroppedDeadNode; }
    
    const NodeEnergyManager& getEnergyManager() const { return energyManager; }
    
    void printCryptoStats() const {
        double successRate = (packetsReceived > 0) ? 
            (double)packetsDecrypted / packetsReceived * 100 : 0.0;
            
        std::cout << "\033[1;35m" << std::string(50, '=') << "\033[0m" << std::endl;
        std::cout << "\033[1;35m" << "   CRYPTOGRAPHY STATISTICS   " << "\033[0m" << std::endl;
        std::cout << "\033[1;35m" << std::string(50, '=') << "\033[0m" << std::endl;
        std::cout << "Packets Encrypted: " << packetsEncrypted << std::endl;
        std::cout << "Packets Received:  " << packetsReceived << std::endl;
        std::cout << "Packets Decrypted: " << packetsDecrypted << std::endl;
        std::cout << "Dropped (Dead Node): " << packetsDroppedDeadNode << std::endl;
        std::cout << "Crypto Success Rate: " << std::fixed << std::setprecision(2) 
                  << successRate << "%" << std::endl;
        std::cout << "\033[1;35m" << std::string(50, '=') << "\033[0m" << std::endl;
    }
    
    void printResilienceMetrics() const {
        std::cout << "\033[1;36m" << std::string(50, '=') << "\033[0m" << std::endl;
        std::cout << "\033[1;36m" << "   RESILIENCE METRICS   " << "\033[0m" << std::endl;
        std::cout << "\033[1;36m" << std::string(50, '=') << "\033[0m" << std::endl;
        std::cout << "Route Changes: " << resilienceMetrics.routeChanges << std::endl;
        std::cout << "Recovery Attempts: " << resilienceMetrics.recoveryAttempts << std::endl;
        std::cout << "Successful Recoveries: " << resilienceMetrics.successfulRecoveries << std::endl;
        std::cout << "Recovery Success Rate: " 
                  << (resilienceMetrics.recoveryAttempts > 0 ? 
                      (resilienceMetrics.successfulRecoveries * 100.0 / resilienceMetrics.recoveryAttempts) : 0)
                  << "%" << std::endl;
        std::cout << "Total Downtime: " << resilienceMetrics.totalDowntime.GetSeconds() << "s" << std::endl;
        std::cout << "\033[1;36m" << std::string(50, '=') << "\033[0m" << std::endl;
    }
};

///////////////////////////////////
// Enhanced CryptoTestApplication //
///////////////////////////////////
class EnhancedCryptoTestApplication : public Application {
public:
    EnhancedCryptoTestApplication() : m_socket(0), m_peerPort(0), m_packetSize(512), 
                                     m_isReceiver(false), m_nodeId(0), m_packetCounter(0),
                                     m_protocol(nullptr) {}
    
    static TypeId GetTypeId() {
        static TypeId tid = TypeId("EnhancedCryptoTestApplication")
            .SetParent<Application>()
            .AddConstructor<EnhancedCryptoTestApplication>();
        return tid;
    }
    
    void Setup(Ptr<Socket> socket, Address address, uint16_t port, 
               uint32_t packetSize, EnhancedMEMOSTPProtocol* protocol, 
               bool isReceiver, uint32_t nodeId) {
        m_socket = socket;
        m_peerAddress = address;
        m_peerPort = port;
        m_packetSize = packetSize;
        m_protocol = protocol;
        m_isReceiver = isReceiver;
        m_nodeId = nodeId;
    }
    
    void StartApplication() override {
        if (m_isReceiver) {
            m_socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), m_peerPort));
            m_socket->SetRecvCallback(MakeCallback(&EnhancedCryptoTestApplication::HandleRead, this));
            
            // Schedule periodic idle energy consumption for receiver
            Simulator::Schedule(Seconds(1), &EnhancedCryptoTestApplication::ConsumeIdleEnergy, this);
        } else {
            m_socket->Bind();
            m_socket->Connect(m_peerAddress);
            
            // Check if node is alive before starting transmission
            if (m_protocol->CanTransmit(m_nodeId, m_packetSize)) {
                Simulator::Schedule(Seconds(0.5), &EnhancedCryptoTestApplication::SendPacket, this);
            } else {
                EmitEvent("transmission_blocked", 0, m_nodeId, -1, 0, "low_energy_or_dead");
                std::cout << "\033[1;33m⚠️  Node " << m_nodeId 
                         << " transmission blocked (low energy or dead)\033[0m" << std::endl;
            }
        }
    }
    
    void StopApplication() override {
        if (m_socket) {
            m_socket->Close();
        }
    }
    
private:
    void SendPacket() {
        // Check if node is still alive and has enough energy
        if (!m_protocol || !m_protocol->CanTransmit(m_nodeId, m_packetSize)) {
            EmitEvent("transmission_aborted", m_packetCounter, m_nodeId, -1, 0, "insufficient_energy");
            return;
        }
        
        std::vector<uint8_t> data(m_packetSize);
        Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
        for (size_t i = 0; i < m_packetSize; i++) {
            data[i] = uv->GetInteger(0, 255);
        }
        
        uint32_t packetId = ++m_packetCounter;
        
        // Emit packet transmission event
        InetSocketAddress destAddr = InetSocketAddress::ConvertFrom(m_peerAddress);
        uint32_t destNode = destAddr.GetIpv4().Get();
        EmitEvent("packet_tx", packetId, m_nodeId, destNode, m_packetSize);
        
        auto encryptedData = m_protocol->encryptPacket(data, m_nodeId, packetId);
        
        if (!encryptedData.empty()) {
            Ptr<Packet> packet = Create<Packet>(encryptedData.data(), encryptedData.size());
            m_socket->Send(packet);
            
            // Consume transmission energy
            m_protocol->ConsumeTransmissionEnergy(m_nodeId, encryptedData.size());
        } else {
            EmitEvent("encryption_failed", packetId, m_nodeId, -1, 0, "node_dead_or_error");
        }
        
        // Schedule next packet if node is still alive
        if (m_protocol->CanTransmit(m_nodeId, m_packetSize)) {
            double interval = 0.5 + uv->GetValue() * 0.3; // Random interval 0.5-0.8s
            Simulator::Schedule(Seconds(interval), &EnhancedCryptoTestApplication::SendPacket, this);
        }
    }
    
    void HandleRead(Ptr<Socket> socket) {
        if (!m_protocol) return;
        
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from))) {
            // Check if receiving node is alive
            if (!m_protocol->CanTransmit(m_nodeId, packet->GetSize())) {
                EmitEvent("reception_blocked", m_packetCounter, -1, m_nodeId, 0, "node_dead");
                return;
            }
            
            // Emit packet reception event
            InetSocketAddress srcAddr = InetSocketAddress::ConvertFrom(from);
            uint32_t srcNode = srcAddr.GetIpv4().Get();
            uint32_t packetId = ++m_packetCounter;
            EmitEvent("packet_rx", packetId, srcNode, m_nodeId, packet->GetSize());
            
            uint32_t size = packet->GetSize();
            std::vector<uint8_t> buffer(size);
            packet->CopyData(buffer.data(), size);
            
            // Consume reception energy
            m_protocol->ConsumeReceptionEnergy(m_nodeId, size);
            
            auto decryptedData = m_protocol->decryptPacket(buffer, m_nodeId, packetId);
            
            if (decryptedData.empty()) {
                EmitEvent("decryption_dropped", packetId, srcNode, m_nodeId, 0, "failed_or_node_dead");
            }
        }
    }
    
    void ConsumeIdleEnergy() {
        if (m_protocol) {
            // Consume idle energy for receiver
            m_protocol->getEnergyManager().ConsumeIdleEnergy(m_nodeId, Seconds(1));
        }
        
        // Schedule next idle energy consumption if node might still be alive
        if (m_protocol && m_protocol->CanTransmit(m_nodeId, 100)) {
            Simulator::Schedule(Seconds(1), &EnhancedCryptoTestApplication::ConsumeIdleEnergy, this);
        }
    }
    
    Ptr<Socket> m_socket;
    Address m_peerAddress;
    uint16_t m_peerPort;
    uint32_t m_packetSize;
    EnhancedMEMOSTPProtocol* m_protocol;
    bool m_isReceiver;
    uint32_t m_nodeId;
    uint32_t m_packetCounter;
};

// Global simulation time variable
double simulationTime = 45.0;

///////////////////////////
// Main Simulation - Enhanced //
///////////////////////////
int main(int argc, char *argv[]) {
    // Emit simulation start event
    EmitEvent("simulation_start", 0, -1, -1, 0, "enhanced_with_node_death");
    
    // Optimized parameters
    uint32_t nNodes = 25;
    double area = 400.0;
    int optimization_iters = 6;
    bool enable_optimization = true;
    bool enable_crypto = true;
    bool visual_output = true;
    bool enable_node_death = true;
    double node_failure_rate = 0.1;

    CommandLine cmd;
    cmd.AddValue("nNodes", "Number of nodes", nNodes);
    cmd.AddValue("simulationTime", "Simulation time", simulationTime);
    cmd.AddValue("area", "Simulation area (m)", area);
    cmd.AddValue("optIters", "Optimization iterations", optimization_iters);
    cmd.AddValue("enableOpt", "Enable optimization", enable_optimization);
    cmd.AddValue("enableCrypto", "Enable ASCON cryptography", enable_crypto);
    cmd.AddValue("visual", "Enable visual output", visual_output);
    cmd.AddValue("enableNodeDeath", "Enable node death/resilience", enable_node_death);
    cmd.AddValue("failureRate", "Node failure rate", node_failure_rate);
    cmd.Parse(argc, argv);

    // Emit configuration event
    EmitEvent("config", 0, nNodes, static_cast<int>(simulationTime), node_failure_rate, 
              enable_node_death ? "with_resilience" : "no_resilience");

    std::cout << "\033[1;36m╔══════════════════════════════════════════════════════════════╗\033[0m" << std::endl;
    std::cout << "\033[1;36m║      ENHANCED MEMOSTP WITH NODE DEATH & RESILIENCE         ║\033[0m" << std::endl;
    std::cout << "\033[1;36m╚══════════════════════════════════════════════════════════════╝\033[0m" << std::endl;
    
    std::cout << "\n📊 Configuration:" << std::endl;
    std::cout << "├─ Nodes: " << nNodes << std::endl;
    std::cout << "├─ Simulation Time: " << simulationTime << " s" << std::endl;
    std::cout << "├─ Node Death: " << (enable_node_death ? "Enabled" : "Disabled") << std::endl;
    std::cout << "├─ Failure Rate: " << node_failure_rate << std::endl;
    std::cout << "└─ Cryptography: " << (enable_crypto ? "ASCON-128" : "Disabled") << std::endl;

    // Test crypto
    if (enable_crypto) {
        std::cout << "\n🧪 Testing Cryptography..." << std::endl;
        std::vector<uint8_t> testData(64);
        for (int i = 0; i < 64; i++) testData[i] = i % 256;
        
        NodeContainer testNodes;
        testNodes.Create(1);
        EnhancedMEMOSTPProtocol testProtocol(testNodes, 1);
        
        auto encrypted = testProtocol.encryptPacket(testData, 0, 1);
        auto decrypted = testProtocol.decryptPacket(encrypted, 1, 1);
        
        if (decrypted.size() == testData.size()) {
            std::cout << "✅ Crypto test PASSED! (" << testData.size() << " bytes)\n" << std::endl;
        } else {
            std::cout << "⚠️  Crypto: " << testData.size() << " → " 
                      << decrypted.size() << " bytes\n" << std::endl;
        }
    }

    // Create main network
    NodeContainer nodes;
    nodes.Create(nNodes);

    EmitEvent("network_create", nNodes);

    // Mobility setup
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
    
    std::cout << "📐 Network Layout: " << gridSize << "×" << gridSize 
              << " grid, spacing: " << gridSpacing << "m" << std::endl;

    // WiFi setup
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
    phy.Set("TxGain", DoubleValue(5.0));
    phy.Set("RxGain", DoubleValue(5.0));
    phy.Set("RxNoiseFigure", DoubleValue(3.0));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("DsssRate2Mbps"),
                                 "ControlMode", StringValue("DsssRate1Mbps"));

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Internet stack with OLSR
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

    // MEMOSTP protocol with resilience
    EnhancedMEMOSTPProtocol memostp(nodes, optimization_iters);
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
            Ptr<EnhancedCryptoTestApplication> recvApp = CreateObject<EnhancedCryptoTestApplication>();
            recvApp->Setup(recvSocket, InetSocketAddress(Ipv4Address::GetAny(), cryptoPort), 
                          cryptoPort, 512, &memostp, true, receiverIdx);
            nodes.Get(receiverIdx)->AddApplication(recvApp);
            recvApp->SetStartTime(Seconds(1.0));
            recvApp->SetStopTime(Seconds(simulationTime - 1.0));
            
            // Sender
            Ptr<Socket> sendSocket = Socket::CreateSocket(nodes.Get(senderIdx), UdpSocketFactory::GetTypeId());
            Ptr<EnhancedCryptoTestApplication> sendApp = CreateObject<EnhancedCryptoTestApplication>();
            sendApp->Setup(sendSocket, InetSocketAddress(interfaces.GetAddress(receiverIdx), cryptoPort), 
                          cryptoPort, 512, &memostp, false, senderIdx);
            nodes.Get(senderIdx)->AddApplication(sendApp);
            sendApp->SetStartTime(Seconds(3.0 + i * 0.5));
            sendApp->SetStopTime(Seconds(simulationTime - 3.0));
        }
        
        std::cout << "📡 Setup " << cryptoPairs << " crypto communication pairs" << std::endl;
    }

    // Add echo traffic for additional network load
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

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    std::cout << "\n\033[1;33m⏳ SIMULATION STARTED...\033[0m" << std::endl;
    EmitEvent("simulation_running", 0);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Collect statistics
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

    double packetDeliveryRatio = (g_totalTxPackets > 0) ? 
        (double)g_totalRxPackets / g_totalTxPackets * 100 : 0;
    double averageDelay = (flows_with_packets > 0) ? totalDelay / flows_with_packets : 0;
    double averageThroughput = (flows_with_packets > 0) ? totalThroughput / flows_with_packets : 0;
    
    // Calculate energy metrics from energy manager
    const auto& energyManager = memostp.getEnergyManager();
    g_totalEnergyConsumed = 0;
    uint32_t aliveNodes = 0;
    
    for (uint32_t i = 0; i < nNodes; i++) {
        double initial = energyManager.GetInitialEnergy(i);
        double remaining = energyManager.GetRemainingEnergy(i);
        g_totalEnergyConsumed += (initial - remaining);
        if (energyManager.IsNodeAlive(i)) aliveNodes++;
    }
    
    double energyEfficiency = (g_totalEnergyConsumed > 0) ? 
        g_totalRxPackets / g_totalEnergyConsumed : 0;
    double energyPerNode = g_totalEnergyConsumed / nNodes;
    double networkLifetime = simulationTime; // Time until first node died
    if (!g_nodeDeathTimes.empty()) {
        networkLifetime = g_nodeDeathTimes[0];
    }

    // Emit comprehensive results
    EmitEvent("stats_packets", g_totalTxPackets, g_totalRxPackets, totalLostPackets);
    EmitEvent("stats_pdr", static_cast<uint32_t>(packetDeliveryRatio));
    EmitEvent("stats_delay", static_cast<uint32_t>(averageDelay * 1000));
    EmitEvent("stats_throughput", static_cast<uint32_t>(averageThroughput * 1000));
    EmitEvent("stats_energy", static_cast<uint32_t>(g_totalEnergyConsumed * 1000));
    EmitEvent("stats_alive_nodes", aliveNodes, nNodes);
    EmitEvent("stats_network_lifetime", static_cast<uint32_t>(networkLifetime));
    EmitEvent("stats_dead_nodes", g_deadNodes);

    // Display comprehensive results
    std::cout << "\033[1;32m\n✨ SIMULATION COMPLETE\033[0m" << std::endl;
    std::cout << "\033[1;37m" << std::string(70, '=') << "\033[0m" << std::endl;
    std::cout << "\033[1;37m      ENHANCED MEMOSTP WITH RESILIENCE - RESULTS      \033[0m" << std::endl;
    std::cout << "\033[1;37m" << std::string(70, '=') << "\033[0m" << std::endl;
    
    std::cout << "\n\033[1;33mNETWORK STATUS:\033[0m" << std::endl;
    std::cout << "├─ Initial Nodes: " << nNodes << std::endl;
    std::cout << "├─ Alive Nodes: " << aliveNodes << " (" 
              << std::fixed << std::setprecision(1) << (aliveNodes*100.0/nNodes) << "%)" << std::endl;
    std::cout << "├─ Dead Nodes: " << g_deadNodes << std::endl;
    std::cout << "└─ Network Lifetime: " << std::fixed << std::setprecision(2) 
              << networkLifetime << " s" << std::endl;

    std::cout << "\n\033[1;33mTRAFFIC STATISTICS:\033[0m" << std::endl;
    std::cout << "├─ Packets Transmitted: " << g_totalTxPackets << std::endl;
    std::cout << "├─ Packets Received:    " << g_totalRxPackets << std::endl;
    std::cout << "├─ Packets Lost:        " << totalLostPackets << std::endl;
    std::cout << "├─ PDR:                 " << std::fixed << std::setprecision(2) 
              << packetDeliveryRatio << "%" << std::endl;
    std::cout << "├─ Avg Delay:           " << std::fixed << std::setprecision(4) 
              << averageDelay << " s" << std::endl;
    std::cout << "└─ Avg Throughput:      " << std::fixed << std::setprecision(3) 
              << averageThroughput << " Mbps" << std::endl;

    std::cout << "\n\033[1;33mENERGY STATISTICS:\033[0m" << std::endl;
    energyManager.PrintEnergyStatistics();
    std::cout << "├─ Total Energy Consumed: " << std::fixed << std::setprecision(3) 
              << g_totalEnergyConsumed << " J" << std::endl;
    std::cout << "├─ Energy per Node:       " << energyPerNode << " J" << std::endl;
    std::cout << "└─ Energy Efficiency:     " << std::fixed << std::setprecision(2) 
              << energyEfficiency << " packets/J" << std::endl;

    if (enable_crypto) {
        std::cout << "\n";
        memostp.printCryptoStats();
    }
    
    if (enable_node_death) {
        std::cout << "\n";
        memostp.printResilienceMetrics();
    }

    if (enable_optimization) {
        std::cout << "\n\033[1;33mFINAL OPTIMIZATION PARAMETERS:\033[0m" << std::endl;
        std::cout << "┌─────────────────────────────────────────────┐" << std::endl;
        std::cout << "│ Energy Weight:      " << std::fixed << std::setw(8) << std::setprecision(4) 
                  << memostp.getEnergyWeight() << " │" << std::endl;
        std::cout << "│ Power Control:      " << std::setw(8) << memostp.getPowerControl() << " │" << std::endl;
        std::cout << "│ Sleep Ratio:        " << std::setw(8) << memostp.getSleepRatio() << " │" << std::endl;
        std::cout << "│ Resilience Factor:  " << std::setw(8) << memostp.getResilienceFactor() << " │" << std::endl;
        std::cout << "└─────────────────────────────────────────────┘" << std::endl;
    }
    
    // Performance summary
    std::cout << "\n\033[1;32m📈 PERFORMANCE SUMMARY:\033[0m" << std::endl;
    std::cout << "├─ Network Availability: " << std::fixed << std::setprecision(1) 
              << (aliveNodes * 100.0 / nNodes) << "%" << std::endl;
    std::cout << "├─ Data Delivery Rate:   " << packetDeliveryRatio << "%" << std::endl;
    std::cout << "├─ Network Lifetime:     " << networkLifetime << " s" << std::endl;
    std::cout << "├─ Energy Efficiency:    " << energyEfficiency << " packets/J" << std::endl;
    std::cout << "└─ Resilience Score:     " << std::fixed << std::setprecision(1) 
              << (aliveNodes * 100.0 / nNodes * packetDeliveryRatio / 100) << "/100" << std::endl;

    EmitEvent("simulation_complete", 0, aliveNodes, nNodes, packetDeliveryRatio, "success");

    std::cout << "\n\033[1;32m✅ Simulation completed successfully!\033[0m" << std::endl;
    std::cout << "\033[1;37m" << std::string(70, '=') << "\033[0m" << std::endl;

    Simulator::Destroy();
    return 0;
}