#include "snake_optimizer.h"
#include "event_emitter.h"

double EnhancedSnakeOptimizer::fitnessFunction(const std::vector<double>& params) {
    // Combined fitness: maximize energy efficiency and network lifetime
    if (params.size() < 3) return 0.0;
    
    double energyWeight = params[0];      // Should be moderate (0.5-0.7)
    double powerControl = params[1];      // Should be high (0.6-0.8)
    double sleepRatio = params[2];        // Should be moderate (0.2-0.4)
    
    // Calculate fitness
    double fitness = 0.0;
    
    // Energy weight component (closer to 0.6 is better)
    fitness += 1.0 - fabs(energyWeight - 0.6);
    
    // Power control component (closer to 0.75 is better)
    fitness += 1.0 - fabs(powerControl - 0.75);
    
    // Sleep ratio component (closer to 0.3 is better)
    fitness += 1.0 - fabs(sleepRatio - 0.3);
    
    // Penalize extreme values
    if (energyWeight < 0.4 || energyWeight > 0.8) fitness *= 0.5;
    if (powerControl < 0.4 || powerControl > 0.9) fitness *= 0.5;
    if (sleepRatio < 0.1 || sleepRatio > 0.5) fitness *= 0.5;
    
    return fitness;
}

void EnhancedSnakeOptimizer::updateSnakePosition(std::vector<double>& params, 
                                                int iteration, int totalIterations) {
    if (params.size() < 3) return;
    
    std::uniform_real_distribution<double> dist(-0.1, 0.1);
    std::normal_distribution<double> normal(0.0, 0.05);
    
    double progress = (double)iteration / totalIterations;
    double exploration = 0.3 * (1.0 - progress);
    double exploitation = 0.7 * progress;
    
    // Update each parameter with snake-like movement
    for (size_t i = 0; i < params.size(); i++) {
        double randomMove = dist(rng) * exploration;
        double guidedMove = (bestParams[i] - params[i]) * exploitation;
        double noise = normal(rng) * (1.0 - progress);
        
        params[i] += randomMove + guidedMove + noise;
        
        // Apply bounds
        if (i == 0) params[i] = std::max(0.4, std::min(0.8, params[i])); // Energy weight
        if (i == 1) params[i] = std::max(0.4, std::min(0.9, params[i])); // Power control
        if (i == 2) params[i] = std::max(0.1, std::min(0.5, params[i])); // Sleep ratio
    }
}

std::vector<double> EnhancedSnakeOptimizer::optimize(int iterations) {
    EventEmitter& emitter = EventEmitter::Instance();
    emitter.EmitEvent("optimization_start", 0);
    
    std::cout << "\033[1;33m🧬 SNAKE OPTIMIZATION STARTED (" << iterations << " iterations)\033[0m" << std::endl;
    
    std::vector<double> currentParams = bestParams;
    double bestFitness = fitnessFunction(bestParams);
    
    for (int iter = 0; iter < iterations; iter++) {
        // Generate new candidate
        std::vector<double> candidate = currentParams;
        updateSnakePosition(candidate, iter, iterations);
        
        double candidateFitness = fitnessFunction(candidate);
        
        // Acceptance criterion
        if (candidateFitness > bestFitness || 
            std::exp((candidateFitness - bestFitness) / (1.0 - (double)iter/iterations)) > 
            std::uniform_real_distribution<double>(0, 1)(rng)) {
            currentParams = candidate;
            if (candidateFitness > bestFitness) {
                bestParams = candidate;
                bestFitness = candidateFitness;
            }
        }
        
        // Emit progress
        if (iter % (iterations/10) == 0 || iter == iterations-1) {
            emitter.EmitEvent("optimization_progress", iter, -1, iterations);
            std::cout << "\033[33m  Iteration " << iter << "/" << iterations 
                      << " | Fitness: " << std::fixed << std::setprecision(4) 
                      << bestFitness << "\033[0m" << std::endl;
        }
    }
    
    emitter.EmitEvent("optimization_complete", iterations);
    std::cout << "\033[1;32m✓ OPTIMIZATION COMPLETE\033[0m" << std::endl;
    
    printOptimizationResults(bestParams);
    return bestParams;
}

void EnhancedSnakeOptimizer::printOptimizationResults(const std::vector<double>& params) const {
    std::cout << "\n\033[1;32m✨ SNAKE OPTIMIZATION RESULTS:\033[0m" << std::endl;
    std::cout << "┌─────────────────────────────────────────────┐" << std::endl;
    std::cout << "│ Energy Weight:   " << std::fixed << std::setw(10) 
              << std::setprecision(4) << getBestEnergyWeight(params) << " │" << std::endl;
    std::cout << "│ Power Control:   " << std::fixed << std::setw(10) 
              << std::setprecision(4) << getBestPowerControl(params) << " │" << std::endl;
    std::cout << "│ Sleep Ratio:     " << std::fixed << std::setw(10) 
              << std::setprecision(4) << getBestSleepRatio(params) << " │" << std::endl;
    std::cout << "└─────────────────────────────────────────────┘" << std::endl;
}