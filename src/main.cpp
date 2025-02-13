#include "global.h"
#include "obj/ISPD24Parser.h"
#include "utils/utils.h"
#include "gr/GlobalRouter.h"
#include "par/Par.h"
#include <mutex>
#include <thread>
#include <global.h>
#include <fstream>

int main(int argc, const char *argv[])
{
    auto start = std::chrono::high_resolution_clock::now();
    // logeol(2);
    // log() << "GLOBAL ROUTER CUGR" << std::endl;
    // logeol(2);
    // Parse parameters
    Parameters parameters(argc, argv);

    // Read CAP/NET
    ISPD24Parser parser(parameters);

    auto par_start = std::chrono::high_resolution_clock::now();

    Par par(parameters, parser);
    std::unordered_map<int, std::vector<std::vector<std::vector<std::tuple<int, int, int>>>>> par_nets;
    std::unordered_map<int, std::vector<std::string>> par_nets_name;
    std::unordered_map<std::string, std::string> par_nets_name_map;
    std::unordered_map<int, GridGraph*> par_grid_graph;
    par.parNets(par_nets, par_nets_name, par_nets_name_map,par_grid_graph);

    double par_time = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - par_start).count();

    std::ofstream par_log("par_time");

    std::unordered_map<int, GridGraph*> routedGridGraph;
    std::unordered_map<int, std::vector<GRNet>> routedNets;

    int area_threads = parameters.area_threads;
    std::mutex mtx;
    par.runJobsMT(area_threads, area_threads, [&](int i){
        GlobalRouter globalRouter(parser, parameters, par_nets_name[i], par_nets[i], par_grid_graph[i]);
        globalRouter.route_multi(mtx, i);
        routedGridGraph[i] = globalRouter.getGridGraph();
        routedNets[i] = globalRouter.getNets();
    });

    auto mergeGridGraph = new GridGraph(parser, parameters);
    for (auto& graph : routedGridGraph)
    {
        mergeGridGraph->merge(*(graph.second));
    }
    
    GlobalRouter globalRouter(parser, parameters, par_nets_name[-1], par_nets[-1], mergeGridGraph);
    globalRouter.route_multi(mtx, -1);
    routedGridGraph[-1] = globalRouter.getGridGraph();
    routedNets[-1] = globalRouter.getNets();
    
    std::unordered_map<std::string, std::vector<GRNet>> outGRNet;
    for (auto& nets : routedNets)
    {
        for (auto& net : nets.second)
        {
            outGRNet[par_nets_name_map[net.getName()]].push_back(net);
        }
    }

    globalRouter.write(parameters.out_file, outGRNet, par.getPin1Net());
    
    double total = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start).count();

    par_log<<par_time<<" "<< total << "\n";
    par_log.close();
    // Global router
    // GlobalRouter globalRouter(parser, parameters);
    // globalRouter.route();
    // globalRouter.write();

    // logeol();
    // log() << "Terminated." << std::endl;
    // loghline();
    // logmem();
    // logeol();
}

// ./route -def ../toys/ispd25-o/ariane.def -v ../toys/ispd25-o/ariane.v.gz -sdc ../toys/ispd25-o/ariane.sdc -cap ../toys/ispd25-o/ariane.cap -net ../toys/ispd25-o/ariane.net -output ../toys/ispd25-o/ariane.guide