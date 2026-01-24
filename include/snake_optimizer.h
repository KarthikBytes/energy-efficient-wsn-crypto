#ifndef SNAKE_OPTIMIZER_H
#define SNAKE_OPTIMIZER_H

#include <vector>
#include <cmath>
#include <random>
#include <iostream>
#include <iomanip>

class EnhancedSnakeOptimizer {
private:
    std::vector<double> bestParams;
    std::mt19937 rng;
    
public:
    EnhancedSnakeOptimizer() : rng(std::random_device{}()) {
        bestParams = {0.6, 0.7, 0.3};
    }
    
    std::vector<double> optimize(int iterations);
    
    double getBestEnergyWeight(const std::vector<double>& params) const { 
        return params.size() > 0 ? params[0] : 0.6; 
    }
    
    double getBestPowerControl(const std::vector<double>& params) const { 
        return params.size() > 1 ? params[1] : 0.7; 
    }
    
    double getBestSleepRatio(const std::vector<double>& params) const { 
        return params.size() > 2 ? params[2] : 0.3; 
    }
    
    void printOptimizationResults(const std::vector<double>& params) const;
    
private:
    double fitnessFunction(const std::vector<double>& params);
    void updateSnakePosition(std::vector<double>& params, int iteration, int totalIterations);
};

#endif // SNAKE_OPTIMIZER_H