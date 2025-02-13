#include "Par.h"
#include <map>
#include <unordered_map>
#include <vector>
#include "Hypergraph.hpp"
#include <tuple>
#include <set>
#include <climits>
#include <boost/polygon/polygon.hpp>
#include <mutex>
#include <thread>

Par::Par(const Parameters &params, const ISPD24Parser &parser) : params(params), parser(parser)
{
    threadNum = params.area_threads;
    net_access_points = parser.net_access_points;
    net_names = parser.net_names;
    netsPinName = parser.netsPinName;
}

// void Par::parNets(std::unordered_map<int, std::vector<std::vector<std::vector<std::tuple<int, int, int>>>>>& par_nets,
//                   std::unordered_map<int, std::vector<std::string>>& par_nets_name,
//                   std::unordered_map<std::string, std::string>& par_nets_name_map,
//                   std::unordered_map<int, GridGraph*>& par_grid_graph)
// {   
//     typedef std::vector<std::vector<std::tuple<int, int, int>>> net; // pin - pin-select

//     std::unordered_map<int, std::vector<int>> hyperEdges;  // netInex - [pinIndex]
//     std::unordered_map<int, std::pair<int, int>> pinCoor;  // pinIndex - (x, y)
//     std::unordered_map<int, std::tuple<int, int, int>> pinIndexPin; // pinIndex - pin
//     std::unordered_map<int, int> netIndexNet; // netIndex - netIndex
//     std::map<std::string, int> pin_name_index;
//     std::map<int, std::string> pin_index_name;

//     std::unordered_map<int, std::vector<net>> netIndexParnet; // netIndex - parNet

//     int global_pin_index = 1;
//     int net_index = 0;
//     for (const auto& db_net : net_access_points){
//         int net_pin_index = 0;
//         for (const auto& pin : db_net){
//             auto select_pin = pin.front();
//             auto pin_name = netsPinName[net_index][net_pin_index];
//             auto result = pin_name_index.find(pin_name);
//             int x = std::get<0>(select_pin);
//             int y = std::get<1>(select_pin);
//             if (result == pin_name_index.end())
//             {
//                 pin_name_index[pin_name] = global_pin_index;
//                 pin_index_name[global_pin_index] = pin_name;
//                 pinCoor[global_pin_index] = std::make_pair(x, y);
//                 pinIndexPin[global_pin_index] = select_pin;
//                 global_pin_index ++;
//             }
//             net_pin_index ++;    
//         }
//         net_index++;
//     }

//     int hyperEdgeIndex = 1;
//     for (const auto& db_net : net_access_points)
//     {
//         int net_pin_index = 0;
//         for (const auto& pin : db_net){
//             auto select_pin = pin.front();
//             int x = std::get<0>(select_pin);
//             int y = std::get<1>(select_pin);
//             auto pin_name = netsPinName[hyperEdgeIndex - 1][net_pin_index];
//             int pinIndex = pin_name_index[pin_name];
//             hyperEdges[hyperEdgeIndex].push_back(pinIndex);
//             net_pin_index ++;    
//         }
//         hyperEdgeIndex ++;
//     }

//     // std::cout<<net_access_points.size()<<std::endl;
//     // std::cout<<hyperEdges.size()<<std::endl;
//     // std::cout<<net_names.size()<<std::endl;

//     std::vector<std::pair<float, float>> Coor_f;
//     for (auto c : pinCoor)
//     {
//         Coor_f.push_back(std::make_pair((float)c.second.first, (float)c.second.second));
//     }

//     Hypergraph hg;
//     hg.NomalizePlacementDat(Coor_f);
//     hg.loadFromTopoFile_uw(hyperEdges, global_pin_index-1);
//     hg.loadFromPlacementFile_uw();
//     hg.K_way_Initial_partition(std::log2(threadNum), hg);
//     std::vector<int> initial_partition = hg.Generate_output(hg);

//     std::unordered_map<int, std::vector<int>> clusters; // parIndex - [pinIndex]
//     std::unordered_map<int, int> pinClusterIndex; // pinIndex - parIndex
//     int index = 1;
//     for(const auto& par : initial_partition){
//         clusters[par].push_back(index);
//         pinClusterIndex[index] = par;
//         index ++;
//     }
    
//     std::unordered_map<int, std::vector<net>> net_par_net; // netIndex - par_net
//     std::unordered_map<std::string, net> par_net; // par_netName - par_net 
//     // std::unordered_map<int, std::vector<net>> par_nets; // parIndex - [par_net]
//     net_index = 0;
//     for (auto& db_net : net_access_points){
//         int net_pin_index = 0;
//         std::unordered_map<int,std::vector<std::tuple<int, int, int>>> net_pins_par; // par - pin
//         for (auto& pin : db_net){
//             auto select_pin = pin.front();
//             int x = std::get<0>(select_pin);
//             int y = std::get<1>(select_pin);
//             // int pinIndex = pin_coor_index[std::make_pair(x, y)];
//             int pinIndex = pin_name_index[netsPinName[net_index][net_pin_index]];
//             int parIndex = pinClusterIndex[pinIndex];
//             net_pins_par[parIndex].push_back(select_pin);
//             net_pin_index ++;
//         }
        
//         for (auto& par : net_pins_par){
//             if (par.second.size() == 1)
//             {
//                 continue;
//             }else
//             {
//                 net new_net_par;
//                 std::string name = net_names[net_index] + "_par" + std::to_string(par.first);
//                 for (auto& p : par.second){
//                     std::vector<std::tuple<int, int, int>> temp;
//                     temp.push_back(p);
//                     new_net_par.push_back(temp);
//                 }
//                 par_nets[par.first].push_back(new_net_par);
//                 par_nets_name[par.first].push_back(name);
//                 net_par_net[net_index].push_back(new_net_par);
//                 par_nets_name_map[name] = net_names[net_index];
//                 netIndexParnet[net_index].push_back(new_net_par);
//             }
//         }

//         if (net_pins_par.size() > 1)
//         {
//             net new_net_par;
//             std::string name = net_names[net_index] + "_par" + "_cross";
//             for (auto& par : net_pins_par){
//                 std::vector<std::tuple<int, int, int>> temp;
//                 temp.push_back(par.second[0]);
//                 new_net_par.push_back(temp);
//             }
//             par_nets[-1].push_back(new_net_par);
//             par_nets_name[-1].push_back(name);
//             net_par_net[net_index].push_back(new_net_par);

//             par_nets_name_map[name] = net_names[net_index];
//             netIndexParnet[net_index].push_back(new_net_par);
//         }

//         if ((net_pins_par.size() == 1)&&(netIndexParnet[net_index].size() == 0))
//         {
//             auto p = net_pins_par.begin()->second.front();
//             pin1net.emplace_back(net_names[net_index],p);
//         }
//         net_index++;
//     }
    
//     std::cout<<netIndexParnet.size() + pin1net.size()<<std::endl;
    
//     std::cin.get();
//     // std::ofstream out("log");
//     // net_index = 0;
//     // for (auto& db_net : net_access_points)
//     // {
//     //     out<<"ori-net name:"<<net_names[net_index]<<std::endl;
//     //     out<<"pin size:"<<db_net.size()<<std::endl;
//     //     for (auto& pin : db_net)
//     //     {
//     //         auto select_pin = pin.front();
//     //         out<<std::get<0>(select_pin)<<" "<<std::get<1>(select_pin)<<" "<<std::get<2>(select_pin)<<"|";
//     //     }
//     //     out<<std::endl;
        
//     //     std::set<std::pair<int,int>> pts;
//     //     auto parNets = netIndexParnet[net_index];
//     //     for (auto& parNet : parNets)
//     //     {
//     //         for (auto& pin : db_net)
//     //         {
//     //             auto select_pin = pin.front();
//     //             pts.insert(std::make_pair(std::get<0>(select_pin), std::get<1>(select_pin)));
//     //             out<<std::get<0>(select_pin)<<" "<<std::get<1>(select_pin)<<" "<<std::get<2>(select_pin)<<"|";
//     //         }
//     //     }
//     //     out<<std::endl;

//     //     if (pts.size() != db_net.size())
//     //     {
//     //         out<<"wrong!"<<std::endl;
//     //     }
//     //     out<<"-------------"<<std::endl;
//     // }
//     std::cin.get();
// }

void Par::runJobsMT(int numJobs, int numofThreads, const std::function<void(int)> &handle)
{
    int numThreads = std::min(numJobs, numofThreads);
    if (numThreads <= 1) {
        for (int i = 0; i < numJobs; ++i) {
            handle(i);
        }
    } else {
        int globalJobIdx = 0;
        std::mutex mtx;
        auto thread_func = [&](int threadIdx) {
            int jobIdx;
            while (true) {
                mtx.lock();
                jobIdx = globalJobIdx++;
                mtx.unlock();
                if (jobIdx >= numJobs) {
                    break;
                }
                handle(jobIdx);
            }
        };

        std::thread threads[numThreads];
        for (int i = 0; i < numThreads; i++) {
            threads[i] = std::thread(thread_func, i);
        }
        for (int i = 0; i < numThreads; i++) {
            threads[i].join();
        }
    }
}

void Par::parNets(std::unordered_map<int, std::vector<std::vector<std::vector<std::tuple<int, int, int>>>>>& par_nets,
                  std::unordered_map<int, std::vector<std::string>>& par_nets_name,
                  std::unordered_map<std::string, std::string>& par_nets_name_map,
                  std::unordered_map<int, GridGraph*>& par_grid_graph)
{
    std::unordered_map<int, std::set<int>> hyperEdges; // hyperIndex - pinIndex
    std::unordered_map<int, std::pair<int, int>> pinCoor; // pinIndex - pin2D coor
    std::map<std::pair<int, int>, int> pinCoorPinIndex; // pinCoor - pinIndex
    std::map<std::string, int> pinNamePinIndex; // pinName - pinIndex

    int global_pin_index = 1;
    std::cout<<"----------------COOR INFO-------------------"<<std::endl;
    for (int netIndex = 0; netIndex < net_access_points.size(); netIndex ++)
    {
        auto net = net_access_points[netIndex];
        for (int netPinIndex = 0; netPinIndex < net.size(); netPinIndex ++)
        {
            auto pin = net[netPinIndex].front();
            int x = std::get<0>(pin);
            int y = std::get<1>(pin);
            auto result = pinCoorPinIndex.find(std::pair(x, y));
            if (result == pinCoorPinIndex.end())
            {
                pinCoor[global_pin_index] = std::pair(x, y);
                pinCoorPinIndex[std::pair(x, y)] = global_pin_index;
                global_pin_index ++;
                pinNamePinIndex[netsPinName[netIndex][netPinIndex]] = global_pin_index;
            }else
            {
                pinNamePinIndex[netsPinName[netIndex][netPinIndex]] = global_pin_index;
            }
            
        }   
    }
    std::cout<<pinCoor.size()<<std::endl;
    
    for (int netIndex = 0; netIndex < net_access_points.size(); netIndex ++)
    {
        auto net = net_access_points[netIndex];
        for (int netPinIndex = 0; netPinIndex < net.size(); netPinIndex ++)
        {
            auto pin = net[netPinIndex].front();
            int x = std::get<0>(pin);
            int y = std::get<1>(pin);
            hyperEdges[netIndex + 1].insert(pinCoorPinIndex[std::pair(x, y)]);
        }
    }

    std::vector<std::pair<float, float>> Coor_f;
    for (const auto& coor : pinCoor)
    {
        Coor_f.push_back(std::make_pair((float)coor.second.first, (float)coor.second.second));
    }

    std::unordered_map<int, std::vector<int>> hyperEdges_v;
    for (auto& hyperEdge : hyperEdges)
    {
        std::vector<int> pins;
        for (auto& pin : hyperEdge.second)
        {
            pins.push_back(pin);
        }
        hyperEdges_v[hyperEdge.first] = pins;
    }

    Hypergraph hg;
    hg.NomalizePlacementDat(Coor_f);
    hg.loadFromTopoFile_uw(hyperEdges_v, global_pin_index-1);
    hg.loadFromPlacementFile_uw();
    cross_cut(hg);
    std::vector<int> initial_partition = hg.Generate_output(hg);

    std::unordered_map<int, std::vector<int>> clusters; // parIndex - pinIndex
    std::unordered_map<int, int> pinClusterIndex; // pinIndex - parIndex
    int index = 1;
    for(const auto& par : initial_partition){
        clusters[par].push_back(index);
        pinClusterIndex[index] = par;
        index ++;
    }

    std::map<int, std::vector<parNet*>> netIndexParNet;
    for (int netIndex = 0; netIndex < net_access_points.size(); netIndex ++)
    {
        auto net = net_access_points[netIndex];
        std::set<std::tuple<int, int, int>> netPinSet;
        for (int netPinIndex = 0; netPinIndex < net.size(); netPinIndex ++)
        {
            auto pin = net[netPinIndex].front();
            netPinSet.insert(pin);
        }

        std::unordered_map<int,std::vector<std::tuple<int, int, int>>> parIndexPin;
        for (auto& pin : netPinSet)
        {
            int x = std::get<0>(pin);
            int y = std::get<1>(pin);
            int parIndex = pinClusterIndex[pinCoorPinIndex[std::pair(x, y)]];
            parIndexPin[parIndex].push_back(pin);
        }
        
        if (parIndexPin.size() == 1)
        {
            auto par = parIndexPin.begin();
            parNet* par_net = new parNet();
            par_net->parNetName = net_names[netIndex] + "_par" + std::to_string(par->first);
            par_net->oriNetName = net_names[netIndex];
            if (par->second.size() == 1)
            {
                // par_net->accessP = net;
                // netIndexParNet[netIndex].push_back(par_net);
                pin1net.emplace_back(net_names[netIndex], par->second.front());
                continue;
            }
            
            for (auto& pin : par->second){
                std::vector<std::tuple<int, int, int>> temp;
                temp.push_back(pin);
                par_net->accessP.push_back(temp);
            }
            par_net->oriNetName = net_names[netIndex];
            netIndexParNet[netIndex].push_back(par_net);
            par_nets[par->first].push_back(par_net->accessP);
            par_nets_name[par->first].push_back(par_net->parNetName);
            par_nets_name_map[par_net->parNetName] = par_net->oriNetName;
            continue;
        }
        
        for (auto& par : parIndexPin)
        {
            if (par.second.size() == 1)
            {
                continue;
            }else
            {
                parNet* par_net = new parNet();
                par_net->parNetName = net_names[netIndex] + "_par" + std::to_string(par.first);
                for (auto& p : par.second){
                    std::vector<std::tuple<int, int, int>> temp;
                    temp.push_back(p);
                    par_net->accessP.push_back(temp);
                }
                par_net->oriNetName = net_names[netIndex];
                netIndexParNet[netIndex].push_back(par_net);
                par_nets[par.first].push_back(par_net->accessP);
                par_nets_name[par.first].push_back(par_net->parNetName);
                par_nets_name_map[par_net->parNetName] = par_net->oriNetName;
            }
        }

        if (parIndexPin.size() > 1)
        {
            parNet* par_net = new parNet();
            par_net->parNetName = net_names[netIndex] + "_cross";
            for (auto& par : parIndexPin){
                std::vector<std::tuple<int, int, int>> temp;
                temp.push_back(par.second[0]);
                par_net->accessP.push_back(temp);
            }
            par_net->oriNetName = net_names[netIndex];
            netIndexParNet[netIndex].push_back(par_net);
            par_nets[-1].push_back(par_net->accessP);
            par_nets_name[-1].push_back(par_net->parNetName);
            par_nets_name_map[par_net->parNetName] = par_net->oriNetName;
        }
    }

    for (size_t i = 0; i < threadNum; i++)
    {
        par_grid_graph[i] = new GridGraph(parser, params);
    }
    
    // std::cout<<netIndexParNet.size()<<std::endl;

    // std::ofstream out("log");
    // for (int netIndex = 0; netIndex < net_access_points.size(); netIndex ++)
    // {
    //     std::set<std::tuple<int, int, int>> oriPins;
    //     std::set<std::tuple<int, int, int>> parPins;
    //     auto net = net_access_points[netIndex];
    //     out<<net_names[netIndex]<<std::endl;
    //     for (int netPinIndex = 0; netPinIndex < net.size(); netPinIndex ++)
    //     {
    //         auto pin = net[netPinIndex].front();
    //         oriPins.insert(pin);
    //         out<<std::get<0>(pin)<<" "<<std::get<1>(pin)<<" "<<std::get<2>(pin)<<"|";
    //     }
    //     out<<std::endl;
    //     out<<"------"<<std::endl;
    //     auto parNets = netIndexParNet[netIndex];
    //     if (parNets.size() > 1)
    //     {
    //         out<<"more than 1"<<std::endl;
    //     }
        
    //     for (auto& parNet : parNets)
    //     {
    //         out<<parNet->oriNetName<<" "<<parNet->parNetName<<std::endl;
    //         for (auto& pin : parNet->accessP)
    //         {
    //             auto select_pin = pin.front();
    //             parPins.insert(select_pin);
    //             out<<std::get<0>(select_pin)<<" "<<std::get<1>(select_pin)<<" "<<std::get<2>(select_pin)<<"|";
    //         }
    //         out<<std::endl;
    //     }
    //     if (oriPins.size() != parPins.size())
    //     {
    //         out<<oriPins.size()<<" "<<parPins.size()<<std::endl;
    //         out<<"wrong!!!"<<std::endl;
    //     }
    //     out<<"------"<<std::endl;
    // }
}
