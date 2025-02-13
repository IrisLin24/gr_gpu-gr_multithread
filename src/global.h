#pragma once
#include <chrono>

std::chrono::high_resolution_clock::time_point program_start;

inline double elapsed_time() {
    std::chrono::duration<double> time_now = std::chrono::high_resolution_clock::now() - program_start;
    return time_now.count(); 
}