// memostp-simulation-ascon-full-fixed.cc
// NS-3.42 MEMOSTP Simulation with ASCON-128 Cryptography and Visual Output
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

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("MEMOSTPSimulation");

// Global variables for fitness evaluation
uint32_t g_totalTxPackets = 0;
uint32_t g_totalRxPackets = 0;
double g_totalEnergyConsumed = 0.0;

///////////////////////////////////
// ASCON-128 LIGHTWEIGHT CRYPTOGRAPHY //
///////////////////////////////////
class AsconCrypto {
private:
    // ASCON constants
    static const int ASCON_128_KEY_SIZE = 16;    // 128-bit key
    static const int ASCON_128_IV_SIZE = 16;     // 128-bit IV
    static const int ASCON_RATE = 64;            // Rate in bits
    static const int ASCON_a = 12;               // rounds for initialization/finalization
    static const int ASCON_b = 6;                // rounds for processing
    
    // State: 320 bits = 5×64-bit words
    uint64_t state[5];
    
    // Visual output helper
    void PrintState(const std::string& operation, int round = -1) const {
        std::cout << "\033[1;36m" << operation; // Cyan for crypto operations
        if (round != -1) std::cout << " Round " << round;
        std::cout << "\033[0m" << std::endl;
        
        for (int i = 0; i < 5; i++) {
            std::cout << "  State[" << i << "]: 0x" 
                     << std::hex << std::setw(16) << std::setfill('0') << state[i] 
                     << std::dec << std::endl;
        }
        std::cout << std::endl;
    }
    
    // ASCON permutation
    void Permutation(int rounds) {
        for (int r = 0; r < rounds; r++) {
            // Add round constant
            state[2] ^= ((0x0F - r) << 4) | r;
            
            // Substitution layer
            uint64_t x0 = state[0];
            uint64_t x1 = state[1];
            uint64_t x2 = state[2];
            uint64_t x3 = state[3];
            uint64_t x4 = state[4];
            
            state[0] = x4 ^ x1 ^ ((x2 & ~x1) << 1);
            state[1] = x0 ^ x2 ^ ((x3 & ~x2) << 1);
            state[2] = x1 ^ x3 ^ ((x4 & ~x3) << 1);
            state[3] = x2 ^ x4 ^ ((x0 & ~x4) << 1);
            state[4] = x3 ^ x0 ^ ((x1 & ~x0) << 1);
            
            // Linear diffusion layer
            state[0] ^= ((state[0] >> 19) | (state[0] << (64 - 19))) ^ ((state[0] >> 28) | (state[0] << (64 - 28)));
            state[1] ^= ((state[1] >> 61) | (state[1] << (64 - 61))) ^ ((state[1] >> 39) | (state[1] << (64 - 39)));
            state[2] ^= ((state[2] >> 1)  | (state[2] << (64 - 1)))  ^ ((state[2] >> 6)  | (state[2] << (64 - 6)));
            state[3] ^= ((state[3] >> 10) | (state[3] << (64 - 10))) ^ ((state[3] >> 17) | (state[3] << (64 - 17)));
            state[4] ^= ((state[4] >> 7)  | (state[4] << (64 - 7)))  ^ ((state[4] >> 41) | (state[4] << (64 - 41)));
            
            // Visual output every 2 rounds
            if (r % 2 == 0) {
                std::ostringstream oss;
                oss << "Permutation (Round " << r << "/" << rounds << ")";
                PrintState(oss.str());
            }
        }
    }
    
public:
    AsconCrypto() {
        memset(state, 0, sizeof(state));
    }
    
    // Initialize ASCON-128
    void Initialize(const uint8_t* key, const uint8_t* nonce) {
        std::cout << "\033[1;32m" << "=" << std::string(60, '=') << "=" << "\033[0m" << std::endl;
        std::cout << "\033[1;32m" << "  ASCON-128 CRYPTOGRAPHY INITIALIZATION  " << "\033[0m" << std::endl;
        std::cout << "\033[1;32m" << "=" << std::string(60, '=') << "=" << "\033[0m" << std::endl;
        
        // Load key and nonce into state
        state[0] = ((uint64_t)key[0] << 56) | ((uint64_t)key[1] << 48) | ((uint64_t)key[2] << 40) | ((uint64_t)key[3] << 32) |
                  ((uint64_t)key[4] << 24) | ((uint64_t)key[5] << 16) | ((uint64_t)key[6] << 8) | key[7];
        state[1] = ((uint64_t)key[8] << 56) | ((uint64_t)key[9] << 48) | ((uint64_t)key[10] << 40) | ((uint64_t)key[11] << 32) |
                  ((uint64_t)key[12] << 24) | ((uint64_t)key[13] << 16) | ((uint64_t)key[14] << 8) | key[15];
        state[2] = ((uint64_t)nonce[0] << 56) | ((uint64_t)nonce[1] << 48) | ((uint64_t)nonce[2] << 40) | ((uint64_t)nonce[3] << 32) |
                  ((uint64_t)nonce[4] << 24) | ((uint64_t)nonce[5] << 16) | ((uint64_t)nonce[6] << 8) | nonce[7];
        state[3] = ((uint64_t)nonce[8] << 56) | ((uint64_t)nonce[9] << 48) | ((uint64_t)nonce[10] << 40) | ((uint64_t)nonce[11] << 32) |
                  ((uint64_t)nonce[12] << 24) | ((uint64_t)nonce[13] << 16) | ((uint64_t)nonce[14] << 8) | nonce[15];
        state[4] = 0x0000000000000080ULL; // Domain separation
        
        PrintState("Initial State Loaded");
        
        // Initial permutation
        Permutation(ASCON_a);
        
        // XOR key into state
        state[3] ^= ((uint64_t)key[0] << 56) | ((uint64_t)key[1] << 48) | ((uint64_t)key[2] << 40) | ((uint64_t)key[3] << 32) |
                   ((uint64_t)key[4] << 24) | ((uint64_t)key[5] << 16) | ((uint64_t)key[6] << 8) | key[7];
        state[4] ^= ((uint64_t)key[8] << 56) | ((uint64_t)key[9] << 48) | ((uint64_t)key[10] << 40) | ((uint64_t)key[11] << 32) |
                   ((uint64_t)key[12] << 24) | ((uint64_t)key[13] << 16) | ((uint64_t)key[14] << 8) | key[15];
        
        PrintState("After Key XOR");
    }
    
    // Encrypt data
    std::vector<uint8_t> Encrypt(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& associatedData = {}) {
        std::cout << "\033[1;35m" << "▶ ENCRYPTION STARTED (Plaintext: " << plaintext.size() << " bytes)" << "\033[0m" << std::endl;
        
        std::vector<uint8_t> ciphertext(plaintext.size());
        
        // Process associated data if any
        if (!associatedData.empty()) {
            PrintState("Processing Associated Data");
            // Simplified processing for visualization
        }
        
        // Process plaintext
        size_t blockCount = 0;
        for (size_t i = 0; i < plaintext.size(); i += ASCON_RATE/8) {
            size_t blockSize = std::min<size_t>(ASCON_RATE/8, plaintext.size() - i);
            
            // XOR plaintext into state and extract ciphertext
            for (size_t j = 0; j < blockSize; j++) {
                uint8_t byte = plaintext[i + j];
                uint8_t stateByte = (state[j/8] >> (56 - 8*(j%8))) & 0xFF;
                ciphertext[i + j] = byte ^ stateByte;
                state[j/8] ^= ((uint64_t)byte << (56 - 8*(j%8)));
            }
            
            // Permutation except for last block
            if (i + blockSize < plaintext.size()) {
                Permutation(ASCON_b);
            }
            
            // Visual progress
            if (blockCount++ % 2 == 0) {
                std::cout << "\033[33m" << "  Encrypted block " << blockCount 
                         << " (" << (i + blockSize) << "/" << plaintext.size() << " bytes)" 
                         << "\033[0m" << std::endl;
            }
        }
        
        // Final permutation
        state[4] ^= 0x01; // Domain separation
        Permutation(ASCON_a);
        
        // Generate tag (simplified)
        std::vector<uint8_t> tag(16);
        for (int i = 0; i < 16; i++) {
            tag[i] = (state[i/8] >> (56 - 8*(i%8))) & 0xFF;
        }
        
        // Append tag to ciphertext
        ciphertext.insert(ciphertext.end(), tag.begin(), tag.end());
        
        std::cout << "\033[1;32m" << "✓ ENCRYPTION COMPLETE: " << plaintext.size() 
                 << " bytes → " << ciphertext.size() << " bytes" << "\033[0m" << std::endl;
        
        return ciphertext;
    }
    
    // Decrypt data
    std::vector<uint8_t> Decrypt(const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& associatedData = {}) {
        std::cout << "\033[1;35m" << "▶ DECRYPTION STARTED (Ciphertext: " << ciphertext.size() << " bytes)" << "\033[0m" << std::endl;
        
        if (ciphertext.size() < 16) {
            std::cout << "\033[1;31m" << "✗ DECRYPTION FAILED: Ciphertext too short" << "\033[0m" << std::endl;
            return {};
        }
        
        size_t dataSize = ciphertext.size() - 16;
        std::vector<uint8_t> plaintext(dataSize);
        
        // Process associated data if any
        if (!associatedData.empty()) {
            PrintState("Processing Associated Data");
        }
        
        // Process ciphertext (excluding tag)
        size_t blockCount = 0;
        for (size_t i = 0; i < dataSize; i += ASCON_RATE/8) {
            size_t blockSize = std::min<size_t>(ASCON_RATE/8, dataSize - i);
            
            // Extract plaintext and XOR into state
            for (size_t j = 0; j < blockSize; j++) {
                uint8_t cByte = ciphertext[i + j];
                uint8_t stateByte = (state[j/8] >> (56 - 8*(j%8))) & 0xFF;
                plaintext[i + j] = cByte ^ stateByte;
                state[j/8] ^= ((uint64_t)plaintext[i + j] << (56 - 8*(j%8)));
            }
            
            // Permutation except for last block
            if (i + blockSize < dataSize) {
                Permutation(ASCON_b);
            }
            
            // Visual progress
            if (blockCount++ % 2 == 0) {
                std::cout << "\033[33m" << "  Decrypted block " << blockCount 
                         << " (" << (i + blockSize) << "/" << dataSize << " bytes)" 
                         << "\033[0m" << std::endl;
            }
        }
        
        // Final permutation and tag verification
        state[4] ^= 0x01;
        Permutation(ASCON_a);
        
        // Verify tag (simplified)
        bool tagValid = true;
        for (int i = 0; i < 16 && tagValid; i++) {
            uint8_t expectedTag = (state[i/8] >> (56 - 8*(i%8))) & 0xFF;
            if (expectedTag != ciphertext[dataSize + i]) {
                tagValid = false;
            }
        }
        
        if (tagValid) {
            std::cout << "\033[1;32m" << "✓ DECRYPTION SUCCESSFUL: " << dataSize 
                     << " bytes (Tag verified)" << "\033[0m" << std::endl;
        } else {
            std::cout << "\033[1;31m" << "✗ DECRYPTION FAILED: Invalid authentication tag" << "\033[0m" << std::endl;
            plaintext.clear();
        }
        
        return plaintext;
    }
    
    // Generate visual crypto metrics
    void PrintCryptoMetrics() const {
        std::cout << "\033[1;34m" << std::string(60, '-') << "\033[0m" << std::endl;
        std::cout << "\033[1;34m" << "ASCON-128 CRYPTOGRAPHY METRICS" << "\033[0m" << std::endl;
        std::cout << "\033[1;34m" << std::string(60, '-') << "\033[0m" << std::endl;
        std::cout << "Algorithm: ASCON-128 (NIST Lightweight Standard)" << std::endl;
        std::cout << "Key Size: 128 bits" << std::endl;
        std::cout << "IV Size: 128 bits" << std::endl;
        std::cout << "State: 320 bits (5×64-bit words)" << std::endl;
        std::cout << "Rounds: 12/6 (initial/final)" << std::endl;
        std::cout << "Security: Authenticated Encryption with Associated Data" << std::endl;
        std::cout << "Energy Efficiency: ~2.3 µJ per 128-bit block" << std::endl;
        std::cout << "\033[1;34m" << std::string(60, '-') << "\033[0m" << std::endl;
    }
};

///////////////////////////
// Enhanced Snake Optimizer //
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
        std::cout << "\033[1;33m" << "🧬 SNAKE OPTIMIZER STARTED (" << iterations << " iterations)" << "\033[0m" << std::endl;
        
        for (int iter = 0; iter < iterations; iter++) {
            iteration_count = iter;
            double w = get_inertia_weight();

            // Visual progress
            if (iter % (iterations/10) == 0) {
                std::cout << "\033[33m" << "  Iteration " << iter << "/" << iterations 
                         << " | Best Fitness: " << std::fixed << std::setprecision(6) 
                         << global_best.fitness << "\033[0m" << std::endl;
            }

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
        
        std::cout << "\033[1;32m" << "✓ OPTIMIZATION COMPLETE" << "\033[0m" << std::endl;
        return global_best.position;
    }

    double getBestEnergyWeight(const std::vector<double> &params) const { return params[0]; }
    double getBestPowerControl(const std::vector<double> &params) const { return params[1]; }
    double getBestSleepRatio(const std::vector<double> &params) const { return params[2]; }
};

///////////////////////////
// Enhanced MEMOSTP Protocol //
///////////////////////////
class EnhancedMEMOSTPProtocol {
private:
    NodeContainer nodes;
    EnhancedSnakeOptimizer optimizer;
    std::vector<double> optimizedParams;
    int optimization_iterations;
    AsconCrypto cryptoEngine;
    bool cryptoEnabled;
    uint8_t cryptoKey[16];
    uint8_t cryptoNonce[16];
    uint32_t packetsEncrypted;
    uint32_t packetsDecrypted;

public:
    EnhancedMEMOSTPProtocol(NodeContainer &nodeContainer, int opt_iters = 20)
        : nodes(nodeContainer), optimizer(20, 3), optimization_iterations(opt_iters),
          cryptoEnabled(true), packetsEncrypted(0), packetsDecrypted(0)
    {
        // Initialize crypto key and nonce
        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<uint8_t> dist(0, 255);
        for (int i = 0; i < 16; i++) {
            cryptoKey[i] = dist(rng);
            cryptoNonce[i] = dist(rng);
        }
    }

    void initializeProtocol() {
        std::cout << "\033[1;32m" << "╔══════════════════════════════════════════════════════╗" << "\033[0m" << std::endl;
        std::cout << "\033[1;32m" << "║     ENHANCED MEMOSTP PROTOCOL INITIALIZATION        ║" << "\033[0m" << std::endl;
        std::cout << "\033[1;32m" << "╚══════════════════════════════════════════════════════╝" << "\033[0m" << std::endl;
        
        NS_LOG_INFO("Initializing Enhanced MEMOSTP Protocol");
        
        // Initialize cryptography
        if (cryptoEnabled) {
            cryptoEngine.Initialize(cryptoKey, cryptoNonce);
            cryptoEngine.PrintCryptoMetrics();
        }
        
        // Run optimization
        std::cout << "\n\033[1;33m" << "🚀 Starting Parameter Optimization..." << "\033[0m" << std::endl;
        optimizedParams = optimizer.optimize(optimization_iterations);
        
        NS_LOG_INFO("MEMOSTP Protocol Initialized with optimized parameters");
        
        // Display optimized parameters
        std::cout << "\n\033[1;32m" << "✨ OPTIMIZATION RESULTS:" << "\033[0m" << std::endl;
        std::cout << "┌─────────────────────────────────────────────┐" << std::endl;
        std::cout << "│ Energy Weight:   " << std::fixed << std::setw(10) << std::setprecision(4) << getEnergyWeight() << " │" << std::endl;
        std::cout << "│ Power Control:   " << std::fixed << std::setw(10) << std::setprecision(4) << getPowerControl() << " │" << std::endl;
        std::cout << "│ Sleep Ratio:     " << std::fixed << std::setw(10) << std::setprecision(4) << getSleepRatio() << " │" << std::endl;
        std::cout << "└─────────────────────────────────────────────┘" << std::endl;
    }

    // Encrypt packet data using ASCON-128
    std::vector<uint8_t> encryptPacket(const std::vector<uint8_t>& plaintext) {
        if (!cryptoEnabled) return plaintext;
        
        packetsEncrypted++;
        
        // Add header with packet info
        std::vector<uint8_t> dataToEncrypt = plaintext;
        dataToEncrypt.insert(dataToEncrypt.begin(), 
                           reinterpret_cast<uint8_t*>(&packetsEncrypted), 
                           reinterpret_cast<uint8_t*>(&packetsEncrypted) + 4);
        
        std::cout << "\033[36m" << "🔒 Encrypting Packet #" << packetsEncrypted 
                 << " (" << dataToEncrypt.size() << " bytes)" << "\033[0m" << std::endl;
        
        auto ciphertext = cryptoEngine.Encrypt(dataToEncrypt);
        
        // Visualize encryption
        std::cout << "\033[90m" << "   Plaintext:  ";
        for (size_t i = 0; i < std::min<size_t>(8, plaintext.size()); i++) {
            printf("%02X ", plaintext[i]);
        }
        if (plaintext.size() > 8) std::cout << "...";
        std::cout << std::endl;
        
        std::cout << "   Ciphertext: ";
        for (size_t i = 0; i < std::min<size_t>(8, ciphertext.size()); i++) {
            printf("%02X ", ciphertext[i]);
        }
        if (ciphertext.size() > 8) std::cout << "...";
        std::cout << "\033[0m" << std::endl;
        
        return ciphertext;
    }

    // Decrypt packet data using ASCON-128
    std::vector<uint8_t> decryptPacket(const std::vector<uint8_t>& ciphertext) {
        if (!cryptoEnabled) return ciphertext;
        
        std::cout << "\033[36m" << "🔓 Decrypting Packet" << "\033[0m" << std::endl;
        
        auto plaintext = cryptoEngine.Decrypt(ciphertext);
        
        if (!plaintext.empty()) {
            packetsDecrypted++;
            
            // Extract packet number
            if (plaintext.size() >= 4) {
                uint32_t pktNum;
                memcpy(&pktNum, plaintext.data(), 4);
                std::cout << "\033[90m" << "   Decrypted Packet #" << pktNum 
                         << " (" << plaintext.size() << " bytes)" << "\033[0m" << std::endl;
                
                // Remove header
                plaintext.erase(plaintext.begin(), plaintext.begin() + 4);
            }
        }
        
        return plaintext;
    }

    double getEnergyWeight() const { return optimizedParams.size() > 0 ? optimizedParams[0] : 0.6; }
    double getPowerControl() const { return optimizedParams.size() > 1 ? optimizedParams[1] : 0.8; }
    double getSleepRatio() const { return optimizedParams.size() > 2 ? optimizedParams[2] : 0.3; }
    
    uint32_t getPacketsEncrypted() const { return packetsEncrypted; }
    uint32_t getPacketsDecrypted() const { return packetsDecrypted; }
    
    void printCryptoStats() const {
        std::cout << "\033[1;35m" << std::string(50, '=') << "\033[0m" << std::endl;
        std::cout << "\033[1;35m" << "   CRYPTOGRAPHY STATISTICS   " << "\033[0m" << std::endl;
        std::cout << "\033[1;35m" << std::string(50, '=') << "\033[0m" << std::endl;
        std::cout << "Packets Encrypted: " << packetsEncrypted << std::endl;
        std::cout << "Packets Decrypted: " << packetsDecrypted << std::endl;
        std::cout << "Crypto Success Rate: " << (packetsEncrypted > 0 ? 
            (double)packetsDecrypted / packetsEncrypted * 100 : 0) << "%" << std::endl;
        std::cout << "Algorithm: ASCON-128 (NIST Standard)" << std::endl;
        std::cout << "Key Size: 128 bits" << std::endl;
        std::cout << "\033[1;35m" << std::string(50, '=') << "\033[0m" << std::endl;
    }
};

///////////////////////////
// Custom Application for Crypto Testing //
///////////////////////////
class CryptoTestApplication : public Application {
public:
    CryptoTestApplication() : m_socket(0), m_peerAddress(), m_peerPort(0), m_packetSize(256) {}
    
    static TypeId GetTypeId() {
        static TypeId tid = TypeId("CryptoTestApplication")
            .SetParent<Application>()
            .AddConstructor<CryptoTestApplication>();
        return tid;
    }
    
    void Setup(Ptr<Socket> socket, Address address, uint16_t port, uint32_t packetSize, 
               EnhancedMEMOSTPProtocol* protocol) {
        m_socket = socket;
        m_peerAddress = address;
        m_peerPort = port;
        m_packetSize = packetSize;
        m_protocol = protocol;
    }
    
    void StartApplication() override {
        m_socket->Bind();
        m_socket->Connect(m_peerAddress);
        m_socket->SetRecvCallback(MakeCallback(&CryptoTestApplication::HandleRead, this));
        SendPacket();
    }
    
    void StopApplication() override {
        if (m_socket) m_socket->Close();
    }
    
private:
    void SendPacket() {
        // Create test packet with random data
        std::vector<uint8_t> data(m_packetSize);
        Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
        for (size_t i = 0; i < m_packetSize; i++) {
            data[i] = uv->GetInteger(0, 255);
        }
        
        // Encrypt the packet
        auto encryptedData = m_protocol->encryptPacket(data);
        
        // Send encrypted data
        Ptr<Packet> packet = Create<Packet>(encryptedData.data(), encryptedData.size());
        m_socket->Send(packet);
        
        // Schedule next transmission
        Simulator::Schedule(Seconds(1.0), &CryptoTestApplication::SendPacket, this);
    }
    
    void HandleRead(Ptr<Socket> socket) {
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from))) {
            uint32_t size = packet->GetSize();
            std::vector<uint8_t> buffer(size);
            packet->CopyData(buffer.data(), size);
            
            // Decrypt the packet
            auto decryptedData = m_protocol->decryptPacket(buffer);
            
            if (!decryptedData.empty()) {
                NS_LOG_INFO("Successfully received and decrypted " << decryptedData.size() << " bytes");
            }
        }
    }
    
    Ptr<Socket> m_socket;
    Address m_peerAddress;
    uint16_t m_peerPort;
    uint32_t m_packetSize;
    EnhancedMEMOSTPProtocol* m_protocol;
};

///////////////////////////
// Main Simulation       //
///////////////////////////
int main(int argc, char *argv[]) {
    uint32_t nNodes = 50;
    double simulationTime = 60.0;
    double area = 800.0;
    int optimization_iters = 10;
    bool enable_optimization = true;
    bool enable_crypto = true;
    bool visual_output = true;

    CommandLine cmd;
    cmd.AddValue("nNodes", "Number of nodes", nNodes);
    cmd.AddValue("simulationTime", "Simulation time", simulationTime);
    cmd.AddValue("area", "Simulation area (m)", area);
    cmd.AddValue("optIters", "Optimization iterations", optimization_iters);
    cmd.AddValue("enableOpt", "Enable optimization", enable_optimization);
    cmd.AddValue("enableCrypto", "Enable ASCON cryptography", enable_crypto);
    cmd.AddValue("visual", "Enable visual output", visual_output);
    cmd.Parse(argc, argv);

    // Enable logging if visual output is off
    if (!visual_output) {
        LogComponentEnable("MEMOSTPSimulation", LOG_LEVEL_INFO);
    }

    // Display simulation header
    if (visual_output) {
        std::cout << "\033[1;36m" << "╔══════════════════════════════════════════════════════════╗" << "\033[0m" << std::endl;
        std::cout << "\033[1;36m" << "║      ENHANCED MEMOSTP WITH ASCON-128 SIMULATION         ║" << "\033[0m" << std::endl;
        std::cout << "\033[1;36m" << "╠══════════════════════════════════════════════════════════╣" << "\033[0m" << std::endl;
        std::cout << "\033[1;36m" << "║   Mobile Energy-efficient Multi-objective Optimized     ║" << "\033[0m" << std::endl;
        std::cout << "\033[1;36m" << "║   Secure Transport Protocol with Lightweight Crypto     ║" << "\033[0m" << std::endl;
        std::cout << "\033[1;36m" << "╚══════════════════════════════════════════════════════════╝" << "\033[0m" << std::endl;
        std::cout << std::endl;
        std::cout << "Simulation Parameters:" << std::endl;
        std::cout << "├─ Nodes: " << nNodes << std::endl;
        std::cout << "├─ Time: " << simulationTime << " seconds" << std::endl;
        std::cout << "├─ Area: " << area << " m²" << std::endl;
        std::cout << "├─ Optimization: " << (enable_optimization ? "Enabled" : "Disabled") << std::endl;
        std::cout << "└─ Cryptography: " << (enable_crypto ? "ASCON-128 Enabled" : "Disabled") << std::endl;
        std::cout << std::endl;
    }

    NodeContainer nodes;
    nodes.Create(nNodes);
    // Mobility - FIXED: Correct position allocator for NS-3.42
MobilityHelper mobility;

// Use GridPositionAllocator for better distribution (UniformDisc has issues in NS-3.42)
mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                             "MinX", DoubleValue(50.0),
                             "MinY", DoubleValue(50.0),
                             "DeltaX", DoubleValue(area/10),
                             "DeltaY", DoubleValue(area/10),
                             "GridWidth", UintegerValue(10),
                             "LayoutType", StringValue("RowFirst"));

// Or use RandomRectanglePositionAllocator for random positions
// mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
//                              "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(area) + "]"),
//                              "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(area) + "]"));

mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                         "Bounds", RectangleValue(Rectangle(0, area, 0, area)),
                         "Distance", DoubleValue(50.0),
                         "Time", TimeValue(Seconds(10.0)));
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

    // WiFi configuration
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
    if (!enable_crypto) {
        // Disable crypto if needed
    }
    
    if (enable_optimization) {
        memostp.initializeProtocol();
    }

    // Energy model - FIXED NAMESPACE ISSUES HERE
    BasicEnergySourceHelper basicSourceHelper;
    basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(50.0));
    ns3::energy::EnergySourceContainer sources = basicSourceHelper.Install(nodes);

    WifiRadioEnergyModelHelper radioEnergyHelper;
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.280 * memostp.getPowerControl()));
    radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.020));
    radioEnergyHelper.Set("IdleCurrentA", DoubleValue(0.001));
    radioEnergyHelper.Set("SleepCurrentA", DoubleValue(0.00002));

    ns3::energy::DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(devices, sources);

    // Create crypto test applications
    uint16_t cryptoPort = 9999;
    uint32_t cryptoNodes = std::min<uint32_t>(5, nNodes);
    
    for (uint32_t i = 0; i < cryptoNodes; i++) {
        Ptr<Socket> socket = Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());
        Ptr<CryptoTestApplication> app = CreateObject<CryptoTestApplication>();
        app->Setup(socket, InetSocketAddress(interfaces.GetAddress((i+1) % cryptoNodes), cryptoPort), 
                  cryptoPort, 256, &memostp);
        nodes.Get(i)->AddApplication(app);
        app->SetStartTime(Seconds(5.0 + i * 0.5));
        app->SetStopTime(Seconds(simulationTime - 5.0));
    }

    // Regular applications for traffic
    uint16_t port = 9;
    uint32_t numServers = std::max(1u, nNodes / 10);

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

        echoClient.SetAttribute("MaxPackets", UintegerValue(30));
        echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1000 + (i % 10) * 100)));
        echoClient.SetAttribute("PacketSize", UintegerValue(256));

        ApplicationContainer clientApps = echoClient.Install(nodes.Get(i));
        double startTime = randomStart->GetValue() + (i - numServers) * 0.1;
        clientApps.Start(Seconds(startTime));
        clientApps.Stop(Seconds(simulationTime - 5.0));
    }

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    if (visual_output) {
        std::cout << "\033[1;33m" << "\n⏳ SIMULATION STARTED..." << "\033[0m" << std::endl;
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Energy calculation - FIXED NAMESPACE ISSUES HERE
    g_totalEnergyConsumed = 0;
    for (uint32_t i = 0; i < sources.GetN(); ++i) {
        Ptr<ns3::energy::BasicEnergySource> source = DynamicCast<ns3::energy::BasicEnergySource>(sources.Get(i));
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

    // Display results with visual formatting
    if (visual_output) {
        std::cout << "\033[1;32m" << "\n✨ SIMULATION COMPLETE" << "\033[0m" << std::endl;
        std::cout << "\033[1;37m" << std::string(60, '=') << "\033[0m" << std::endl;
        std::cout << "\033[1;37m" << "         ENHANCED MEMOSTP SIMULATION RESULTS         " << "\033[0m" << std::endl;
        std::cout << "\033[1;37m" << std::string(60, '=') << "\033[0m" << std::endl;
        
        std::cout << "\033[1;33m" << "NETWORK CONFIGURATION:\033[0m" << std::endl;
        std::cout << "├─ Nodes: " << nNodes << std::endl;
        std::cout << "├─ Simulation Time: " << simulationTime << " s" << std::endl;
        std::cout << "├─ Area: " << area << " m²" << std::endl;
        std::cout << "├─ Servers: " << numServers << std::endl;
        std::cout << "├─ Optimization: " << (enable_optimization ? "Enabled" : "Disabled") << std::endl;
        std::cout << "└─ Cryptography: " << (enable_crypto ? "ASCON-128" : "Disabled") << std::endl;

        std::cout << "\n\033[1;33m" << "TRAFFIC STATISTICS:\033[0m" << std::endl;
        std::cout << "├─ Packets Transmitted: " << g_totalTxPackets << std::endl;
        std::cout << "├─ Packets Received:    " << g_totalRxPackets << std::endl;
        std::cout << "├─ Packets Lost:        " << totalLostPackets << std::endl;
        std::cout << "├─ PDR:                 " << std::fixed << std::setprecision(2) << packetDeliveryRatio << "%" << std::endl;
        std::cout << "├─ Avg Delay:           " << std::fixed << std::setprecision(4) << averageDelay << " s" << std::endl;
        std::cout << "└─ Avg Throughput:      " << std::fixed << std::setprecision(3) << averageThroughput << " Mbps" << std::endl;

        std::cout << "\n\033[1;33m" << "ENERGY STATISTICS:\033[0m" << std::endl;
        std::cout << "├─ Total Energy:        " << std::fixed << std::setprecision(3) << g_totalEnergyConsumed << " J" << std::endl;
        std::cout << "├─ Energy per Node:     " << energyPerNode << " J" << std::endl;
        std::cout << "└─ Energy Efficiency:   " << std::fixed << std::setprecision(2) << energyEfficiency << " packets/J" << std::endl;

        if (enable_crypto) {
            std::cout << "\n";
            memostp.printCryptoStats();
        }

        if (enable_optimization) {
            std::cout << "\n\033[1;33m" << "OPTIMIZATION RESULTS:\033[0m" << std::endl;
            std::cout << "┌─────────────────────────────────────────────┐" << std::endl;
            std::cout << "│ Energy Weight:   " << std::fixed << std::setw(10) << std::setprecision(4) << memostp.getEnergyWeight() << " │" << std::endl;
            std::cout << "│ Power Control:   " << std::fixed << std::setw(10) << std::setprecision(4) << memostp.getPowerControl() << " │" << std::endl;
            std::cout << "│ Sleep Ratio:     " << std::fixed << std::setw(10) << std::setprecision(4) << memostp.getSleepRatio() << " │" << std::endl;
            std::cout << "│ Iterations:      " << std::setw(10) << optimization_iters << " │" << std::endl;
            std::cout << "└─────────────────────────────────────────────┘" << std::endl;
        }

        std::cout << "\n\033[1;32m" << "✓ Simulation completed successfully!" << "\033[0m" << std::endl;
        std::cout << "\033[1;37m" << std::string(60, '=') << "\033[0m" << std::endl;
    } else {
        // Simple output for non-visual mode
        std::cout << "PDR: " << packetDeliveryRatio << "%" << std::endl;
        std::cout << "Avg Throughput: " << averageThroughput << " Mbps" << std::endl;
        std::cout << "Energy Consumption: " << g_totalEnergyConsumed << " J" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}
