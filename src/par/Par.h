#pragma once

#include "global.h"
#include "../gr/GridGraph.h"
#include <tuple>

struct parNet {
    std::string parNetName;
    std::string oriNetName;
    std::vector<std::vector<std::tuple<int, int, int>>> accessP;
    int par;
};

class Par{
public:
    Par(const Parameters& params, const ISPD24Parser& parser);
    void parNets(std::unordered_map<int, std::vector<std::vector<std::vector<std::tuple<int, int, int>>>>>& par_nets, 
                 std::unordered_map<int, std::vector<std::string>>& par_nets_name,
                 std::unordered_map<std::string, std::string>& par_nets_name_map,
                 std::unordered_map<int, GridGraph*>& par_grid_graph);
    void runJobsMT(int numJobs, int numofThreads, const std::function<void(int)> &handle);
    // void parNets();
    std::vector<std::pair<std::string,std::tuple<int, int, int>>> getPin1Net() { return pin1net; }
private:
    int threadNum;
    std::vector<std::vector<std::vector<std::tuple<int, int, int>>>> net_access_points;
    std::vector<std::string> net_names;
    std::vector<std::vector<std::string>> netsPinName;
    Parameters params;
    ISPD24Parser parser;  
    std::vector<std::pair<std::string,std::tuple<int, int, int>>> pin1net;
};
