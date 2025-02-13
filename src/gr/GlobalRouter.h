#pragma once
#include "GridGraph.h"
#include "GRNet.h"
#include "../obj/ISPD24Parser.h"
#include "../multithread/Scheduler.h" 
#include "../multithread/SingleNetRouter.h"
#include <mutex>

class GlobalRouter {
public:
    GlobalRouter(const ISPD24Parser& parser, const Parameters& params);
    GlobalRouter(const ISPD24Parser& parser, const Parameters &params, std::vector<std::string> net_names, std::vector<std::vector<std::vector<std::tuple<int, int, int>>>> net_access_points, GridGraph* _gridGraph);
    
    void route();
    void write(std::string guide_file = "");

    void update_nonstack_via_counter(unsigned net_idx, const std::vector<std::vector<int>> &via_loc, std::vector<std::vector<std::vector<int>>> &flag, std::vector<std::vector<std::vector<int>>> &nonstack_via_counter) const;

    GridGraph* getGridGraph() const { return gridGraph; };
    const std::vector<GRNet>& getNets() const { return nets; };

    void route_multi(std::mutex& areamtx, int threadId);
    void write(std::string guide_file , std::unordered_map<std::string, std::vector<GRNet>> outGRNet, std::vector<std::pair<std::string,std::tuple<int, int, int>>> pin1net);
    void getGuide(const GRNet &net, std::vector<std::array<int, 6>> &guide);
private:
    std::vector<std::string> pin0net;
    std::vector<DBU> horizontal_gcell_edge_lengths;
    std::vector<DBU> vertical_gcell_edge_lengths;
    const Parameters& parameters;
    GridGraph* gridGraph;
    std::vector<GRNet> nets;
    
    int areaOfPinPatches;
    int areaOfWirePatches;

    int numofThreads;

    // for evaluation
    CostT unit_length_wire_cost;
    CostT unit_via_cost;
    std::vector<CostT> unit_length_short_costs;
    
    void sortNetIndices(std::vector<int>& netIndices) const; 
    void sortNetIndicesD(std::vector<int> &netIndices) const;
    void sortNetIndicesOFD(std::vector<int> &netIndices, std::vector<int> &netOverflow) const;
    void sortNetIndicesOFDALD(std::vector<int> &netIndices, std::vector<int> &netOverflow) const;
    void sortNetIndicesOLD(std::vector<int> &netIndices) const;
    void sortNetIndicesOLI(std::vector<int> &netIndices) const;
    void sortNetIndicesRandom(std::vector<int> &netIndices) const;
    void getGuides(const GRNet &net, std::vector<std::pair<std::pair<int, int>, utils::BoxT<int>>> &guides);
    // void getGuides(const GRNet &net, std::vector<std::pair<int, utils::BoxT<int>>> &guides);
    void printStatistics() const;

    void runJobsMT(int numJobs, int numofThreads, const std::function<void(int)>& handle);
    void runJobsMTnew(std::vector<std::vector<int>> batches, const std::function<void(int)>& handle);
    std::vector<std::vector<int>> getBatches(std::vector<SingleNetRouter>& routers, const std::vector<int>& netsToRoute);
};