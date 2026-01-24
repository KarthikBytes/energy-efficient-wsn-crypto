#include "event_emitter.h"
#include <iomanip>
#include <iostream>

void EventEmitter::EmitEvent(const std::string& event, uint32_t packetId, int from, int to) {
    std::lock_guard<std::mutex> lock(mtx);
    
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    
    std::cout << "{"
              << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()).count() << ","
              << "\"time\":" << std::fixed << std::setprecision(3) << simulationStartTime << ","
              << "\"event\":\"" << event << "\","
              << "\"packetId\":" << packetId;
    
    if (from >= 0)
        std::cout << ",\"from\":" << from;
    if (to >= 0)
        std::cout << ",\"to\":" << to;
    
    std::cout << "}" << std::endl;
}

void EventEmitter::EmitNodeEvent(uint32_t nodeId, const std::string& status, double energy) {
    std::lock_guard<std::mutex> lock(mtx);
    
    std::cout << "{"
              << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch()).count() << ","
              << "\"type\":\"node_event\","
              << "\"nodeId\":" << nodeId << ","
              << "\"status\":\"" << status << "\"";
    
    if (energy >= 0)
        std::cout << ",\"energy\":" << std::fixed << std::setprecision(3) << energy;
    
    std::cout << "}" << std::endl;
}

void EventEmitter::EmitMetric(const std::string& metric, double value, const std::string& unit) {
    std::lock_guard<std::mutex> lock(mtx);
    metrics[metric].push_back(value);
    
    std::cout << "{"
              << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch()).count() << ","
              << "\"type\":\"metric\","
              << "\"metric\":\"" << metric << "\","
              << "\"value\":" << std::fixed << std::setprecision(6) << value;
    
    if (!unit.empty())
        std::cout << ",\"unit\":\"" << unit << "\"";
    
    std::cout << "}" << std::endl;
}

void EventEmitter::SetSimulationStartTime() {
    simulationStartTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() / 1000.0;
}

void EventEmitter::LogNodeDeath(uint32_t nodeId, double deathTime, const std::string& cause) {
    std::lock_guard<std::mutex> lock(mtx);
    
    nodeDeaths.push_back({nodeId, deathTime});
    
    if (firstNodeDeathTime < 0 || deathTime < firstNodeDeathTime)
        firstNodeDeathTime = deathTime;
    
    if (deathTime > lastNodeDeathTime)
        lastNodeDeathTime = deathTime;
    
    EmitNodeEvent(nodeId, "dead", 0.0);
    EmitEvent("node_death", nodeId, nodeId, -1);
    
    std::cout << "{"
              << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch()).count() << ","
              << "\"type\":\"node_death\","
              << "\"nodeId\":" << nodeId << ","
              << "\"deathTime\":" << std::fixed << std::setprecision(3) << deathTime << ","
              << "\"cause\":\"" << cause << "\""
              << "}" << std::endl;
}

void EventEmitter::PrintDeathStatistics() const {
    std::lock_guard<std::mutex> lock(mtx);
    
    if (nodeDeaths.empty()) {
        std::cout << "\033[1;32mNo node deaths recorded.\033[0m" << std::endl;
        return;
    }
    
    std::cout << "\n\033[1;31m💀 NODE DEATH STATISTICS:\033[0m" << std::endl;
    std::cout << "\033[1;37m" << std::string(50, '=') << "\033[0m" << std::endl;
    
    std::cout << "Total Deaths: " << nodeDeaths.size() << std::endl;
    std::cout << "First Death:  " << std::fixed << std::setprecision(2) 
              << firstNodeDeathTime << "s" << std::endl;
    std::cout << "Last Death:   " << lastNodeDeathTime << "s" << std::endl;
    std::cout << "Death Spread: " << (lastNodeDeathTime - firstNodeDeathTime) << "s" << std::endl;
    
    std::cout << "\nDeath Timeline:" << std::endl;
    for (const auto& death : nodeDeaths) {
        std::cout << "  Node " << death.first << " died at " 
                  << std::fixed << std::setprecision(2) << death.second << "s" << std::endl;
    }
    
    std::cout << "\033[1;37m" << std::string(50, '=') << "\033[0m" << std::endl;
}