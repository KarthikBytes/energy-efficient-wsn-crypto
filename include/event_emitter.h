#ifndef EVENT_EMITTER_H
#define EVENT_EMITTER_H

#include <iostream>
#include <iomanip>
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <chrono>
#include <ctime>
#include <sstream>

class EventEmitter {
public:
    static EventEmitter& Instance() {
        static EventEmitter instance;
        return instance;
    }
    
    void EmitEvent(const std::string& event, uint32_t packetId, int from = -1, int to = -1);
    void EmitNodeEvent(uint32_t nodeId, const std::string& status, double energy = -1.0);
    void EmitMetric(const std::string& metric, double value, const std::string& unit = "");
    
    void SetSimulationStartTime();
    void LogNodeDeath(uint32_t nodeId, double deathTime, const std::string& cause);
    
    double GetFirstNodeDeathTime() const { return firstNodeDeathTime; }
    double GetLastNodeDeathTime() const { return lastNodeDeathTime; }
    const std::vector<std::pair<uint32_t, double>>& GetNodeDeaths() const { return nodeDeaths; }
    
    void PrintDeathStatistics() const;
    
private:
    EventEmitter() : simulationStartTime(0.0), firstNodeDeathTime(-1.0), lastNodeDeathTime(-1.0) {}
    EventEmitter(const EventEmitter&) = delete;
    EventEmitter& operator=(const EventEmitter&) = delete;
    
    double simulationStartTime;
    double firstNodeDeathTime;
    double lastNodeDeathTime;
    std::vector<std::pair<uint32_t, double>> nodeDeaths;
    std::map<std::string, std::vector<double>> metrics;
    mutable std::mutex mtx;
};

#endif // EVENT_EMITTER_H