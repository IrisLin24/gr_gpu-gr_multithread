#include "GlobalRouter.h"
#include "PatternRoute.h"
#include "MazeRoute.h"
#include "GPUMazeRoute.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include "GridGraph.h"
#include "../obj/ISPD24Parser.h"
#include <cmath>
#include <fstream>
#include <mutex>
#include <thread>
// #include "../multithread/tp.cpp"
// #include <boost/asio/static_thread_pool.hpp>
// #include <boost/asio.hpp>

using std::max;
using std::min;
using std::vector;

GlobalRouter::GlobalRouter(const ISPD24Parser &parser, const Parameters &params)
    : parameters(params),vertical_gcell_edge_lengths(parser.vertical_gcell_edge_lengths), horizontal_gcell_edge_lengths(parser.horizontal_gcell_edge_lengths),pin0net(parser.pin0net)
{
    gridGraph = new GridGraph(parser, params);
    nets.reserve(parser.net_names.size());
    for (int i = 0; i < parser.net_names.size(); i++)
        nets.emplace_back(i, parser.net_names[i], parser.net_access_points[i]);
    unit_length_wire_cost = parser.unit_length_wire_cost;
    unit_via_cost = parser.unit_via_cost;
    unit_length_short_costs = parser.unit_length_short_costs;
    numofThreads = params.inner_threads;
}

GlobalRouter::GlobalRouter(const ISPD24Parser &parser, const Parameters &params, std::vector<std::string> net_names, std::vector<std::vector<std::vector<std::tuple<int, int, int>>>> net_access_points, GridGraph* _gridGraph)
    : parameters(params),vertical_gcell_edge_lengths(parser.vertical_gcell_edge_lengths), horizontal_gcell_edge_lengths(parser.horizontal_gcell_edge_lengths),pin0net(parser.pin0net)
{
    gridGraph = _gridGraph;
    nets.reserve(net_names.size());
    for (int i = 0; i < net_names.size(); i++)
        nets.emplace_back(i, net_names[i], net_access_points[i]);
    unit_length_wire_cost = parser.unit_length_wire_cost;
    unit_via_cost = parser.unit_via_cost;
    unit_length_short_costs = parser.unit_length_short_costs;
    // numofThreads = params.threads;
    numofThreads = 4;
}

void GlobalRouter::route()
{
    int n1 = 0, n2 = 0, n3 = 0;
    double t1 = 0, t2 = 0, t3 = 0;

    auto t = std::chrono::high_resolution_clock::now();
    // std::ofstream  afile;
    // afile.open("time", std::ios::app);
    // std::ofstream  afile;
    // afile.open("time", std::ios::app);

    vector<int> netIndices;
    vector<int> netOverflows(nets.size());
    netIndices.reserve(nets.size());
    for (const auto &net : nets)
        netIndices.push_back(net.getIndex());
    // Stage 1: Pattern routing
    n1 = netIndices.size();
    PatternRoute::readFluteLUT();
    // log() << "stage 1: pattern routing" << std::endl;
    sortNetIndices(netIndices);

    auto t_b_b = std::chrono::high_resolution_clock::now();
    vector<SingleNetRouter> routers;
    routers.reserve(netIndices.size());
    for (auto id : netIndices)
        routers.emplace_back(nets[id]);
    
    vector<vector<int>> batches = getBatches(routers, netIndices);
    double t_batch = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t_b_b).count();
    // std::ofstream outputFile("batches.txt");
    // if (outputFile.is_open())
    // {
    //     for (const auto &batch : batches)
    //     {
    //         for (const auto &value : batch)
    //         {
    //             outputFile << nets[value].getBoundingBox().hp() << " ";
    //         }
    //         outputFile << std::endl;
    //     }
    //     outputFile.close();
    // }
    // else
    // {
    //     // Failed to open the file
    //     // Handle the error accordingly
    // }

    // std::cout<<"b:"<<batches.size()<<std::endl;

    // for (const int netIndex : netIndices)
    // {
    //     PatternRoute patternRoute(nets[netIndex], gridGraph, parameters);
    //     patternRoute.constructSteinerTree();
    //     patternRoute.constructRoutingDAG();
    //     patternRoute.run();
    //     gridGraph->commitTree(nets[netIndex].getRoutingTree());
    // }

    // std::vector<PatternRoute*> PatternRoutes;
    auto t_cst_b = std::chrono::high_resolution_clock::now();
    std::unordered_map<int, PatternRoute> PatternRoutes;
    // for output SteinerTree.txt
    // std::ofstream cfile;
    // cfile.open("SteinerTree.txt", std::ios::app);
    // cfile << "[";
    for (const int netIndex : netIndices)
    {
        PatternRoute patternRoute(nets[netIndex], *gridGraph, parameters);
        patternRoute.constructSteinerTree();
        // if(SteinerTreeNode::getPythonString(patternRoute.getSteinerTree())!="[]"){
        //     cfile <<SteinerTreeNode::getPythonString(patternRoute.getSteinerTree())<<","<<"\n";
        // }
        // patternRoute.constructRoutingDAG();
        PatternRoutes.insert(std::make_pair(netIndex, patternRoute));
    }
    //cfile << "]";
    //cfile.close();
    double t_cst = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t_cst_b).count();

    auto t_pr_b = std::chrono::high_resolution_clock::now();
    for (const vector<int> &batch : batches)
    {
        runJobsMT(batch.size(), numofThreads, [&](int jobIdx)
                  {
            auto patternRoute = PatternRoutes.find(batch[jobIdx])->second;
            patternRoute.constructRoutingDAG();
            patternRoute.run();
            gridGraph->commitTree(nets[batch[jobIdx]].getRoutingTree()); });
    }
    // gridGraph->clearDemand();
    // for (auto net:nets)
    // {
    //     gridGraph->commitTree(net.getRoutingTree());
    // }
    
    double t_pr = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t_pr_b).count();

    // netIndices.clear();
    // for (const auto &net : nets)
    // {
    //     int netOverflow = gridGraph->checkOverflow(net.getRoutingTree());
    //     if (netOverflow > 0)
    //     {
    //         netIndices.push_back(net.getIndex());
    //         netOverflows[net.getIndex()] = netOverflow;
    //     }
    // }
    // std::cout<<"before:"<<netIndices.size()<<std::endl;

    // sortNetIndices(netIndices);
    // for (const int netIndex : netIndices){
    //     GRNet &net = nets[netIndex];
    //     gridGraph->commitTree(net.getRoutingTree(), true);
    //     //auto patternRoute = PatternRoutes.find(netIndex)->second;
    //     PatternRoute patternRoute(net, gridGraph, parameters);
    //     patternRoute.constructSteinerTree();
    //     patternRoute.constructRoutingDAG();
    //     patternRoute.run();
    //     gridGraph->commitTree(net.getRoutingTree());
    // }

    netIndices.clear();
    for (const auto &net : nets)
    {
        int netOverflow = gridGraph->checkOverflow(net.getRoutingTree());
        if (netOverflow > 0)
        {
            netIndices.push_back(net.getIndex());
            netOverflows[net.getIndex()] = netOverflow;
        }
    }
    // std::cout<<"after:"<<netIndices.size()<<std::endl;
    // afile << netIndices.size() << " ";
    // log() << netIndices.size() << " / " << nets.size() << " nets have overflows." << std::endl;
    logeol();
    // afile<<netIndices.size()<<" ";
    // printStatistics();
    t1 = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t).count();
    t = std::chrono::high_resolution_clock::now();

    // Stage 2: Pattern routing with possible detours
    n2 = netIndices.size();
    if (netIndices.size() > 0)
    {
        log() << "stage 2: pattern routing with possible detours" << std::endl;
        GridGraphView<bool> congestionView; // (2d) direction -> x -> y -> has overflow?
        gridGraph->extractCongestionView(congestionView);
        // for (const int netIndex : netIndices) {
        //     GRNet& net = nets[netIndex];
        //     gridGraph->commitTree(net.getRoutingTree(), true);
        // }
#ifndef ENABLE_ISSSORT
        sortNetIndices(netIndices);
#else
        log() << "sort net indices with OFDALD" << std::endl;
        sortNetIndicesOFDALD(netIndices, netOverflows);
        // sortNetIndicesD(netIndices);
#endif
        // for (const int netIndex : netIndices)
        // {
        //     GRNet &net = nets[netIndex];
        //     gridGraph->commitTree(net.getRoutingTree(), true);
        //     PatternRoute patternRoute(net, gridGraph, parameters);
        //     patternRoute.constructSteinerTree();
        //     patternRoute.constructRoutingDAG();
        //     patternRoute.constructDetours(congestionView); // KEY DIFFERENCE compared to stage 1
        //     patternRoute.run();
        //     gridGraph->commitTree(net.getRoutingTree());
        // }

        routers.clear();
        batches.clear();
        PatternRoutes.clear();
    
        // log()<<"routers size:"<<routers.size()<<std::endl;
        // log()<<"batches size:"<<batches.size()<<std::endl;
        // log()<<"PatternRoutes size:"<<PatternRoutes.size()<<std::endl;
        routers.reserve(netIndices.size());
        for (auto id : netIndices)
            routers.emplace_back(nets[id]);
        batches = getBatches(routers, netIndices);

        // std::ofstream  aafile;
        // aafile.open("batch", std::ios::app);
        
        // for (auto batch: batches)
        // {
        //     for (auto id: batch)
        //     {
        //         aafile<<nets[id].getBoundingBox()<<" ";
        //     }
        //     aafile<<"\n";
        // }
        for (const int netIndex : netIndices)
        {
            GRNet &net = nets[netIndex];
            // gridGraph->commitTree(net.getRoutingTree(), true);
            PatternRoute patternRoute(net, *gridGraph, parameters);
            patternRoute.constructSteinerTree();
            // patternRoute.constructRoutingDAG();
            // patternRoute.constructDetours(congestionView); // KEY DIFFERENCE compared to stage 1
            PatternRoutes.insert(std::make_pair(netIndex, patternRoute));   
        }
        // // log()<<"routers size1:"<<routers.size()<<std::endl;
        // // log()<<"batches size1:"<<batches.size()<<std::endl;
        // // log()<<"PatternRoutes size1:"<<PatternRoutes.size()<<std::endl;

        // // std::ofstream outputFile("batches.txt");
        // // if (outputFile.is_open())
        // // {
        // //     for (const auto &batch : batches)
        // //     {
        // //         for (const auto &value : batch)
        // //         {
        // //             outputFile << value << " ";
        // //         }
        // //         outputFile << std::endl;
        // //     }
        // //     outputFile.close();
        // // }
        // // else
        // // {
        // //     // Failed to open the file
        // //     // Handle the error accordingly
        // // }

        //std::mutex mtx;
        for (const vector<int> &batch : batches)
        {
            runJobsMT(batch.size(), numofThreads, [&](int jobIdx)
                      {
            GRNet &net = nets[batch[jobIdx]];
            gridGraph->commitTree(net.getRoutingTree(), true);
            auto patternRoute = PatternRoutes.find(batch[jobIdx])->second;
            patternRoute.constructRoutingDAG();
            patternRoute.constructDetours(congestionView); // KEY DIFFERENCE compared to stage 1
            patternRoute.run();
            //mtx.lock();
            gridGraph->commitTree(net.getRoutingTree()); 
            //mtx.unlock();
            });
        }

        // std::vector<std::vector<std::vector<GraphEdge>>> old_demand;
        // std::vector<std::vector<std::vector<GraphEdge>>> new_demand;
        // std::vector<double> result;
        // std::ofstream  dfile;
        // dfile.open("demand", std::ios::app);
        // old_demand = gridGraph->get(); 
        // gridGraph->clearDemand();
        // for (auto net:nets)
        // {
        //     gridGraph->commitTree(net.getRoutingTree());
        // }
        // new_demand = gridGraph->get();
        // int count1 = 0;
        // for (size_t i = 0; i < old_demand.size(); i++)
        // {
        //     for (size_t j = 0; j < old_demand[i].size(); j++)
        //     {
        //         for (size_t k = 0; k < old_demand[i][j].size(); k++)
        //         {
        //             result.emplace_back(new_demand[i][j][k].demand - old_demand[i][j][k].demand);
        //             count1 ++;
        //         }
        //     }
        // }
        
        // dfile<<"total_demand_num:"<<count1<<std::endl;
        // double count2=0;
        // int total_num=0;
        // for (auto i:result)
        // {
        //     total_num += i;
        //     // if (i > 0)
        //     // {
        //     //     total_num += i;
        //     if(i !=0) 
        //     {dfile<<i<<" ";
        //     count2 ++;}
        //     // }else if (i < 0)
        //     // {
        //     //     std::cout<<"negative"<<"\n";
        //     // }
        // }
        // dfile<<std::endl;
        // dfile<<"wrong_demand_num:"<<count2<<std::endl;
        // dfile<<"average_wrong_value"<<total_num/count2<<std::endl;
        // std::cout<<"num"<<std::endl;
        // dfile.close();
        // std::cin.get();

        // gridGraph->clearDemand();
        // for (auto net : nets)
        // {
        //     gridGraph->commitTree(net.getRoutingTree());
        // }

        routers.clear();
        batches.clear();
        PatternRoutes.clear();

        netIndices.clear();
        for (const auto &net : nets)
        {
            int netOverflow = gridGraph->checkOverflow(net.getRoutingTree());
            if (netOverflow > 0)
            {
                netIndices.push_back(net.getIndex());
                netOverflows[net.getIndex()] = netOverflow;
                // log() << "netindex: " << net.getIndex() << " netoverflow: " << netOverflow << std::endl;
            }
        }

        // afile << netIndices.size() << " ";
        // log() << netIndices.size() << " / " << nets.size() << " nets have overflows." << std::endl;
        // logeol();
        // afile<<netIndices.size()<<" ";
    }
    // printStatistics();
    t2 = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t).count();
    t = std::chrono::high_resolution_clock::now();

    // Stage 3: maze routing
    n3 = netIndices.size();
#ifndef ENABLE_CUDA
    if (netIndices.size() > 0)
    {
        log() << "stage 3: maze routing on sparsified routing graph" << std::endl;
        for (const int netIndex : netIndices)
        {
            GRNet &net = nets[netIndex];
            gridGraph->commitTree(net.getRoutingTree(), true);
        }
        GridGraphView<CostT> wireCostView;
        gridGraph->extractWireCostView(wireCostView);
#ifndef ENABLE_ISSSORT
        sortNetIndices(netIndices);
#else
        log() << "sort net indices with OFDALD" << std::endl;
        sortNetIndicesOFDALD(netIndices, netOverflows);
#endif
        SparseGrid grid(10, 10, 0, 0);
        for (const int netIndex : netIndices)
        {
            GRNet &net = nets[netIndex];
            // gridGraph->commitTree(net.getRoutingTree(), true);
            // gridGraph->updateWireCostView(wireCostView, net.getRoutingTree());
            MazeRoute mazeRoute(net, *gridGraph, parameters);
            mazeRoute.constructSparsifiedGraph(wireCostView, grid);
            mazeRoute.run();
            std::shared_ptr<SteinerTreeNode> tree = mazeRoute.getSteinerTree();
            assert(tree != nullptr);

            PatternRoute patternRoute(net, *gridGraph, parameters);
            patternRoute.setSteinerTree(tree);
            patternRoute.constructRoutingDAG();
            patternRoute.run();

            gridGraph->commitTree(net.getRoutingTree());
            gridGraph->updateWireCostView(wireCostView, net.getRoutingTree());
            grid.step();
        }
        netIndices.clear();
        for (const auto &net : nets)
        {
            if (gridGraph->checkOverflow(net.getRoutingTree()) > 0)
            {
                netIndices.push_back(net.getIndex());
            }
        }
        // afile << netIndices.size() << " ";
        // log() << netIndices.size() << " / " << nets.size() << " nets have overflows." << std::endl;
        // logeol();
        // afile<<netIndices.size()<<" ";
    }
#else
    if (netIndices.size() > 0)
    {
        log() << "stage 3: gpu maze routing\n";
        GPUMazeRoute mazeRoute(nets, gridGraph, parameters);
        mazeRoute.run();
        mazeRoute.getOverflowNetIndices(netIndices);
        log() << netIndices.size() << " / " << nets.size() << " nets have overflows." << std::endl;
    }
#endif
    t3 = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t).count();
    t = std::chrono::high_resolution_clock::now();

    // log() << "step routed #nets: " << n1 << ", " << n2 << ", " << n3 << "\n";
    // log() << "step time consumption: "
    //       << std::setprecision(3) << std::fixed << t1 << " s, "
    //       << std::setprecision(3) << std::fixed << t2 << " s, "
    //       << std::setprecision(3) << std::fixed << t3 << " s\n";
    
    // afile<< t_batch << " " << t_cst << " " << t_pr << " ";
    // afile<< t1 << " " << t2 << " " << t3 << " ";
    // afile.close();
    // printStatistics();
    // if (parameters.write_heatmap)
    //     gridGraph->write();
}

void GlobalRouter::sortNetIndices(vector<int> &netIndices) const
{
    vector<int> halfParameters(nets.size());
    for (int netIndex : netIndices)
    {
        auto &net = nets[netIndex];
        halfParameters[netIndex] = net.getBoundingBox().hp();
    }
    sort(netIndices.begin(), netIndices.end(), [&](int lhs, int rhs)
         { return halfParameters[lhs] < halfParameters[rhs]; });
}

void GlobalRouter::sortNetIndicesD(vector<int> &netIndices) const
{
    vector<int> halfParameters(nets.size());
    for (int netIndex : netIndices)
    {
        auto &net = nets[netIndex];
        halfParameters[netIndex] = net.getBoundingBox().hp();
    }
    sort(netIndices.begin(), netIndices.end(), [&](int lhs, int rhs)
         { return halfParameters[lhs] > halfParameters[rhs]; });
}

void GlobalRouter::sortNetIndicesOFD(vector<int> &netIndices, vector<int> &netOverflow) const
{
    sort(netIndices.begin(), netIndices.end(), [&](int lhs, int rhs)
         { return netOverflow[lhs] > netOverflow[rhs]; });
}

void GlobalRouter::sortNetIndicesOFDALD(vector<int> &netIndices, vector<int> &netOverflow) const
{
    vector<int> scores(nets.size());
    for (int netIndex : netIndices)
    {
        auto &net = nets[netIndex];
        scores[netIndex] = net.getBoundingBox().hp() + 30 * netOverflow[netIndex];
    }
    sort(netIndices.begin(), netIndices.end(), [&](int lhs, int rhs)
         { return scores[lhs] > scores[rhs]; });
}

void GlobalRouter::sortNetIndicesOLD(vector<int> &netIndices) const
{
    vector<int> scores(nets.size());
    for (int netIndex : netIndices)
    {
        int overlapNum = 0;
        auto &net = nets[netIndex];
        for (int otherNetIndex : netIndices)
        {
            if (otherNetIndex == netIndex)
                continue;
            auto &otherNet = nets[otherNetIndex];
            bool overlap = net.overlap(otherNet);
            if (overlap)
                overlapNum++;
        }
        int halfParameter = net.getBoundingBox().hp();
        scores[netIndex] = overlapNum / (halfParameter + 2);
        // netScores[netIndex] = overlapNum/halfParameter+2;
    }
    sort(netIndices.begin(), netIndices.end(), [&](int lhs, int rhs)
         { return scores[lhs] > scores[rhs]; });
}

void GlobalRouter::sortNetIndicesOLI(vector<int> &netIndices) const
{
    vector<int> scores(nets.size());
    for (int netIndex : netIndices)
    {
        int overlapNum = 0;
        auto &net = nets[netIndex];
        for (int otherNetIndex : netIndices)
        {
            if (otherNetIndex == netIndex)
                continue;
            auto &otherNet = nets[otherNetIndex];
            bool overlap = net.overlap(otherNet);
            if (overlap)
                overlapNum++;
        }
        int halfParameter = net.getBoundingBox().hp();
        scores[netIndex] = overlapNum / (halfParameter + 2);
        // netScores[netIndex] = overlapNum/halfParameter+2;
    }
    sort(netIndices.begin(), netIndices.end(), [&](int lhs, int rhs)
         { return scores[lhs] > scores[rhs]; });
}

// void GlobalRouter::sortNetIndicesRandom(vector<int>& netIndices) const {
//     // 使用 std::random_device 获取随机种子
//     std::random_device rd;

//     // 使用随机种子初始化 std::mt19937 引擎
//     std::mt19937 gen(rd());

//     // 使用 std::shuffle 对 vector 元素进行随机排序
//     std::shuffle(netIndices.begin(), netIndices.end(), gen);
// }

// void GlobalRouter::getGuides(const GRNet& net, vector<std::pair<int, utils::BoxT<int>>>& guides) {
//     auto& routingTree = net.getRoutingTree();
//     if (!routingTree) return;
//     // 0. Basic guides
//     GRTreeNode::preorder(routingTree, [&](std::shared_ptr<GRTreeNode> node) {
//         for (const auto& child : node->children) {
//             if (node->layerIdx == child->layerIdx) {
//                 guides.emplace_back(
//                     node->layerIdx, utils::BoxT<int>(
//                         min(node->x, child->x), min(node->y, child->y),
//                         max(node->x, child->x), max(node->y, child->y)
//                     )
//                 );
//             } else {
//                 int maxLayerIndex = max(node->layerIdx, child->layerIdx);
//                 for (int layerIdx = min(node->layerIdx, child->layerIdx); layerIdx <= maxLayerIndex; layerIdx++) {
//                     guides.emplace_back(layerIdx, utils::BoxT<int>(node->x, node->y));
//                 }
//             }
//         }
//     });

//     auto getSpareResource = [&] (const GRPoint& point) {
//         double resource = std::numeric_limits<double>::max();
//         unsigned direction = gridGraph->getLayerDirection(point.layerIdx);
//         if (point[direction] + 1 < gridGraph->getSize(direction)) {
//             resource = min(resource, gridGraph->getEdge(point.layerIdx, point.x, point.y).getResource());
//         }
//         if (point[direction] > 0) {
//             GRPoint lower = point;
//             lower[direction] -= 1;
//             resource = min(resource, gridGraph->getEdge(lower.layerIdx, point.x, point.y).getResource());
//         }
//         return resource;
//     };

//     // 1. Pin access patches
//     assert(parameters.min_routing_layer + 1 < gridGraph->getNumLayers());
//     for (auto& gpts : net.getPinAccessPoints()) {
//         for (auto& gpt : gpts) {
//             if (gpt.layerIdx < parameters.min_routing_layer) {
//                 int padding = 0;
//                 if (getSpareResource({parameters.min_routing_layer, gpt.x, gpt.y}) < parameters.pin_patch_threshold) {
//                     padding = parameters.pin_patch_padding;
//                 }
//                 for (int layerIdx = gpt.layerIdx; layerIdx <= parameters.min_routing_layer + 1; layerIdx++) {
//                     guides.emplace_back(layerIdx, utils::BoxT<int>(
//                         max(gpt.x - padding, 0),
//                         max(gpt.y - padding, 0),
//                         min(gpt.x + padding, (int)gridGraph->getSize(0) - 1),
//                         min(gpt.y + padding, (int)gridGraph->getSize(1) - 1)
//                     ));
//                     areaOfPinPatches += (guides.back().second.x.range() + 1) * (guides.back().second.y.range() + 1);
//                 }
//             }
//         }
//     }

//     // 2. Wire segment patches
//     GRTreeNode::preorder(routingTree, [&](std::shared_ptr<GRTreeNode> node) {
//         for (const auto& child : node->children) {
//             if (node->layerIdx == child->layerIdx) {
//                 double wire_patch_threshold = parameters.wire_patch_threshold;
//                 unsigned direction = gridGraph->getLayerDirection(node->layerIdx);
//                 int l = min((*node)[direction], (*child)[direction]);
//                 int h = max((*node)[direction], (*child)[direction]);
//                 int r = (*node)[1 - direction];
//                 for (int c = l; c <= h; c++) {
//                     bool patched = false;
//                     GRPoint point = (direction == MetalLayer::H ? GRPoint(node->layerIdx, c, r) : GRPoint(node->layerIdx, r, c));
//                     if (getSpareResource(point) < wire_patch_threshold) {
//                         for (int layerIndex = node->layerIdx - 1; layerIndex <= node->layerIdx + 1; layerIndex += 2) {
//                             if (layerIndex < parameters.min_routing_layer || layerIndex >= gridGraph->getNumLayers()) continue;
//                             if (getSpareResource({layerIndex, point.x, point.y}) >= 1.0) {
//                                 guides.emplace_back(layerIndex, utils::BoxT<int>(point.x, point.y));
//                                 areaOfWirePatches += 1;
//                                 patched = true;
//                             }
//                         }
//                     }
//                     if (patched) {
//                         wire_patch_threshold = parameters.wire_patch_threshold;
//                     } else {
//                         wire_patch_threshold *= parameters.wire_patch_inflation_rate;
//                     }
//                 }
//             }
//         }
//     });
// }

void GlobalRouter::getGuides(const GRNet &net, vector<std::pair<std::pair<int, int>, utils::BoxT<int>>> &guides)
{
    auto &routingTree = net.getRoutingTree();
    if (!routingTree)
        return;
    // 0. Basic guides
    GRTreeNode::preorder(routingTree, [&](std::shared_ptr<GRTreeNode> node)
                         {
        for (const auto& child : node->children) {
            if (node->layerIdx == child->layerIdx) {
                if(min(node->x, child->x)!=max(node->x, child->x) || min(node->y, child->y)!=max(node->y, child->y)){
                    guides.emplace_back(
                        std::make_pair(node->layerIdx,node->layerIdx), utils::BoxT<int>(
                            min(node->x, child->x), min(node->y, child->y),
                            max(node->x, child->x), max(node->y, child->y)
                        )
                    );
                }
            } else {
                int maxLayerIndex = max(node->layerIdx, child->layerIdx);
                int minLayerIndex = min(node->layerIdx, child->layerIdx);
                guides.emplace_back(std::make_pair(minLayerIndex, maxLayerIndex), utils::BoxT<int>(node->x, node->y));
            }
        } });

    auto getSpareResource = [&](const GRPoint &point)
    {
        double resource = std::numeric_limits<double>::max();
        unsigned direction = gridGraph->getLayerDirection(point.layerIdx);
        if (point[direction] + 1 < gridGraph->getSize(direction))
        {
            resource = min(resource, gridGraph->getEdge(point.layerIdx, point.x, point.y).getResource());
        }
        if (point[direction] > 0)
        {
            GRPoint lower = point;
            lower[direction] -= 1;
            resource = min(resource, gridGraph->getEdge(lower.layerIdx, point.x, point.y).getResource());
        }
        return resource;
    };
}

void GlobalRouter::printStatistics() const
{
    log() << "routing statistics" << std::endl;
    loghline();

    // wire length and via count
    uint64_t wireLength = 0;
    int viaCount = 0;
    vector<vector<vector<int>>> wireUsage;
    vector<vector<vector<int>>> nonstack_via_counter;
    vector<vector<vector<int>>> flag;
    wireUsage.assign(
        gridGraph->getNumLayers(), vector<vector<int>>(gridGraph->getSize(0), vector<int>(gridGraph->getSize(1), 0)));
    nonstack_via_counter.assign(
        gridGraph->getNumLayers(), vector<vector<int>>(gridGraph->getSize(0), vector<int>(gridGraph->getSize(1), 0)));
    flag.assign(
        gridGraph->getNumLayers(), vector<vector<int>>(gridGraph->getSize(0), vector<int>(gridGraph->getSize(1), -1)));
    for (const auto &net : nets)
    {
        vector<vector<int>> via_loc;
        if (net.getRoutingTree() == nullptr)
        {
            log() << "ERROR: null GRTree net(id=" << net.getIndex() << "\n";
            exit(-1);
        }
        GRTreeNode::preorder(net.getRoutingTree(), [&](std::shared_ptr<GRTreeNode> node)
                             {
            for (const auto& child : node->children) {
                if (node->layerIdx == child->layerIdx) {
                    unsigned direction = gridGraph->getLayerDirection(node->layerIdx);
                    int l = min((*node)[direction], (*child)[direction]);
                    int h = max((*node)[direction], (*child)[direction]);
                    int r = (*node)[1 - direction];
                    for (int c = l; c < h; c++) {
                        wireLength += gridGraph->getEdgeLength(direction, c);
                        int x = direction == MetalLayer::H ? c : r;
                        int y = direction == MetalLayer::H ? r : c;
                        wireUsage[node->layerIdx][x][y] += 1;
                        flag[node->layerIdx][x][y] = net.getIndex();
                    }
                    int x = direction == MetalLayer::H ? h : r;
                    int y = direction == MetalLayer::H ? r : h;
                    flag[node->layerIdx][x][y] = net.getIndex();
                } else {
                        int minLayerIndex = min(node->layerIdx, child->layerIdx);
                        int maxLayerIndex = max(node->layerIdx, child->layerIdx);
                        for (int layerIdx = minLayerIndex; layerIdx < maxLayerIndex; layerIdx++) {
                            via_loc.push_back({node->x, node->y, layerIdx});
                        }
                    viaCount += abs(node->layerIdx - child->layerIdx);
                }
            } });
        update_nonstack_via_counter(net.getIndex(), via_loc, flag, nonstack_via_counter);
    }

    // resource
    CapacityT overflow_cost = 0;
    double overflow_slope = 0.5;

    CapacityT minResource = std::numeric_limits<CapacityT>::max();
    GRPoint bottleneck(-1, -1, -1);

    for (unsigned z = parameters.min_routing_layer; z < gridGraph->getNumLayers(); z++)
    {
        unsigned long long num_overflows = 0;
        unsigned long long total_wl = 0;
        double layer_overflows = 0;
        double overflow = 0;
        unsigned layer_nonstack_via_counter = 0;
        for (unsigned x = 0; x < gridGraph->getSize(0); x++)
        {
            for (unsigned y = 0; y < gridGraph->getSize(1); y++)
            {
                layer_nonstack_via_counter += nonstack_via_counter[z][x][y];
                int usage = 2 * wireUsage[z][x][y] + nonstack_via_counter[z][x][y];
                double capacity = max(gridGraph->getEdge(z, x, y).capacity, 0.0);
                if (usage > 2 * capacity)
                {
                    num_overflows += usage - 2 * capacity;
                }

                if (capacity > 0)
                {
                    overflow = double(usage) - 2 * double(capacity);
                    layer_overflows += exp((overflow / 2) * overflow_slope);
                }
                else if (capacity == 0 && usage > 0)
                {
                    layer_overflows += exp(1.5 * double(usage) * overflow_slope);
                }
                else if (capacity < 0)
                {
                    printf("Capacity error (%d, %d, %d)\n", x, y, z);
                }
            }
        }
        overflow_cost += layer_overflows * 0.1; // gg.unit_overflow_cost();
        // log() << "Layer = " << z << " layer_nonstack_via_counter: "<<layer_nonstack_via_counter<< ", num_overflows = " << num_overflows << ", layer_overflows = " << layer_overflows << ", overflow cost = " << overflow_cost << std::endl;
        log() << "Layer = " << z << ", num_overflows = " << num_overflows << ", layer_overflows = " << layer_overflows << ", overflow cost = " << overflow_cost << std::endl;
    }

    double via_cost_scale = 1.0;
    double overflow_cost_scale = 1.0;
    double wireCost = wireLength * unit_length_wire_cost;
    double viaCost = viaCount * unit_via_cost * via_cost_scale;
    double overflowCost = overflow_cost * overflow_cost_scale;
    double totalCost = wireCost + viaCost + overflowCost;

    log() << "wire cost:                " << wireCost << std::endl;
    log() << "via cost:                 " << viaCost << std::endl;
    log() << "overflow cost:            " << overflowCost << std::endl;
    log() << "total cost(ispd24 score): " << totalCost << std::endl;
    // std::ofstream  afile;
    // afile.open("time", std::ios::app);
    // afile<<wireCost<<" "<<viaCost<<" "<<overflowCost<<" "<<totalCost<<"\n";
    // afile.close();
    // logeol();
}

void GlobalRouter::write(std::string guide_file)
{
    log() << "generating route guides..." << std::endl;
    if (guide_file == "")
        guide_file = parameters.out_file;

    areaOfPinPatches = 0;
    areaOfWirePatches = 0;
    std::stringstream ss;
    for (const GRNet &net : nets)
    {
        vector<std::pair<std::pair<int, int>, utils::BoxT<int>>> guides;
        getGuides(net, guides);

        ss << net.getName() << std::endl;
        ss << "(" << std::endl;
        for (const auto &guide : guides)
        {
            ss << guide.second.x.low << " "
               << guide.second.y.low << " "
               << guide.first.first << " "
               << guide.second.x.high << " "
               << guide.second.y.high << " "
               << guide.first.second << std::endl;
        }
        ss << ")" << std::endl;
    }
    log() << std::endl;
    log() << "writing output..." << std::endl;
    std::ofstream fout(guide_file);
    fout << ss.str();
    fout.close();
    log() << "finished writing output..." << std::endl;
}

void GlobalRouter::update_nonstack_via_counter(unsigned net_idx,
                                               const std::vector<vector<int>> &via_loc,
                                               std::vector<std::vector<std::vector<int>>> &flag,
                                               std::vector<std::vector<std::vector<int>>> &nonstack_via_counter) const
{
    for (const auto &pp : via_loc)
    {
        if (flag[pp[2]][pp[0]][pp[1]] != net_idx)
        {
            flag[pp[2]][pp[0]][pp[1]] = net_idx;

            int direction = gridGraph->getLayerDirection(pp[2]);
            int size = gridGraph->getSize(direction);
            if (direction == 0)
            {
                if ((pp[0] > 0) && (pp[0] < size - 1))
                {
                    nonstack_via_counter[pp[2]][pp[0] - 1][pp[1]]++;
                    nonstack_via_counter[pp[2]][pp[0]][pp[1]]++;
                }
                else if (pp[0] > 0)
                {
                    nonstack_via_counter[pp[2]][pp[0] - 1][pp[1]] += 2;
                }
                else if (pp[0] < size - 1)
                {
                    nonstack_via_counter[pp[2]][pp[0]][pp[1]] += 2;
                }
            }
            else if (direction == 1)
            {
                if ((pp[1] > 0) && (pp[1] < size - 1))
                {
                    nonstack_via_counter[pp[2]][pp[0]][pp[1] - 1]++;
                    nonstack_via_counter[pp[2]][pp[0]][pp[1]]++;
                }
                else if (pp[1] > 0)
                {
                    nonstack_via_counter[pp[2]][pp[0]][pp[1] - 1] += 2;
                }
                else if (pp[1] < size - 1)
                {
                    nonstack_via_counter[pp[2]][pp[0]][pp[1]] += 2;
                }
            }
        }
    }
}

vector<vector<int>> GlobalRouter::getBatches(vector<SingleNetRouter> &routers, const vector<int> &netsToRoute)
{
    vector<int> batch(netsToRoute.size());
    vector<vector<int>> batches;
    if (numofThreads == 1)
    {
        batches.emplace_back(netsToRoute);
        return batches;
    }
    else
    {
        // runJobsMT(netsToRoute.size(), numofThreads, [&](int jobIdx)
        //           {
        // auto& router = routers[jobIdx];
        // const auto mergedPinAccessBoxes = nets[netsToRoute[jobIdx]].getPinAccessPoints();
        // //const auto mergedPinAccessBoxes = router.grNet.getPinAccessPoints();
        // utils::IntervalT<long int> xIntvl, yIntvl;
        // for (auto& points : mergedPinAccessBoxes) {
        //     for (auto& point : points) {
        //         xIntvl.Update(point[0]);
        //         yIntvl.Update(point[1]);
        //     }
        // }
        // router.guides.emplace_back(0, xIntvl, yIntvl); });

        runJobsMT(netsToRoute.size(), numofThreads, [&](int jobIdx) {
            auto& router = routers[jobIdx];
            auto& net = router.grNet;
            utils::IntervalT<long int> xIntvl, yIntvl;
            xIntvl.low = DBU(net.getBoundingBox().x.low);
            xIntvl.high = DBU(net.getBoundingBox().x.high);
            yIntvl.low = DBU(net.getBoundingBox().y.low);
            yIntvl.high = DBU(net.getBoundingBox().y.high);
            router.guides.emplace_back(0, xIntvl, yIntvl);
        });

        Scheduler scheduler(routers, gridGraph->getNumLayers());
        batches = scheduler.scheduleOrderEq(numofThreads, netsToRoute);
    }
    return batches;
}

void GlobalRouter::runJobsMT(int numJobs, int numofThreads, const std::function<void(int)> &handle)
{
    int numThreads = min(numJobs, numofThreads);
    if (numThreads <= 1)
    {
        for (int i = 0; i < numJobs; ++i)
        {
            handle(i);
        }
    }
    else
    {
        int globalJobIdx = 0;
        std::mutex mtx;
        auto thread_func = [&](int threadIdx)
        {
            int jobIdx;
            while (true)
            {
                mtx.lock();
                jobIdx = globalJobIdx++;
                mtx.unlock();
                if (jobIdx >= numJobs)
                {
                    break;
                }
                handle(jobIdx);
            }
        };

        std::thread threads[numThreads];
        for (int i = 0; i < numThreads; i++)
        {
            threads[i] = std::thread(thread_func, i);
        }
        for (int i = 0; i < numThreads; i++)
        {
            threads[i].join();
        }
    }
}

// for (const vector<int>& batch : batches) {
//         runJobsMT(batch.size(),numofThreads, [&](int jobIdx) {
//             auto patternRoute = PatternRoutes.find(batch[jobIdx])->second;
//             //auto patternRoute = PatternRoutes[batch[jobIdx]];
//             patternRoute.run();
//             gridGraph->commitTree(nets[batch[jobIdx]].getRoutingTree());
//         });
//    }

void GlobalRouter::runJobsMTnew(std::vector<std::vector<int>> batches, const std::function<void(int)> &handle)
{
    auto thread_func = [&](std::vector<int> batch)
    {
        for (auto id : batch)
        {
            handle(id);
        }
    };
    std::thread threads[numofThreads];
    for (int i = 0; i < numofThreads; i++)
    {
        threads[i] = std::thread(thread_func, batches[i]);
    }
    for (int i = 0; i < numofThreads; i++)
    {
        threads[i].join();
    }
}

void GlobalRouter::route_multi(std::mutex& areamtx, int threadId)
{ 
    int n1 = 0, n2 = 0, n3 = 0;
    double t1 = 0, t2 = 0, t3 = 0;

    auto t = std::chrono::high_resolution_clock::now();
    std::string time_file = "time" + std::to_string(threadId);
    std::ofstream afile(time_file);

    vector<int> netIndices;
    vector<int> netOverflows(nets.size());
    netIndices.reserve(nets.size());
    for (const auto &net : nets)
        netIndices.push_back(net.getIndex());
    // Stage 1: Pattern routing
    n1 = netIndices.size();
    areamtx.lock();
    PatternRoute::readFluteLUT();
    areamtx.unlock();
    log() << "stage 1: pattern routing" << std::endl;
    sortNetIndices(netIndices);

    auto t_b_b = std::chrono::high_resolution_clock::now();
    vector<SingleNetRouter> routers;
    routers.reserve(netIndices.size());
    for (auto id : netIndices)
        routers.emplace_back(nets[id]);
    
    vector<vector<int>> batches = getBatches(routers, netIndices);
    double t_batch = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t_b_b).count();
    // std::ofstream outputFile("batches.txt");
    // if (outputFile.is_open())
    // {
    //     for (const auto &batch : batches)
    //     {
    //         for (const auto &value : batch)
    //         {
    //             outputFile << nets[value].getBoundingBox().hp() << " ";
    //         }
    //         outputFile << std::endl;
    //     }
    //     outputFile.close();
    // }
    // else
    // {
    //     // Failed to open the file
    //     // Handle the error accordingly
    // }

    // std::cout<<"b:"<<batches.size()<<std::endl;

    // for (const int netIndex : netIndices)
    // {
    //     PatternRoute patternRoute(nets[netIndex], gridGraph, parameters);
    //     patternRoute.constructSteinerTree();
    //     patternRoute.constructRoutingDAG();
    //     patternRoute.run();
    //     gridGraph->commitTree(nets[netIndex].getRoutingTree());
    // }

    // std::vector<PatternRoute*> PatternRoutes;
    auto t_cst_b = std::chrono::high_resolution_clock::now();
    std::unordered_map<int, PatternRoute> PatternRoutes;
    // for output SteinerTree.txt
    // std::ofstream cfile;
    // cfile.open("SteinerTree.txt", std::ios::app);
    // cfile << "[";
    for (const int netIndex : netIndices)
    {
        PatternRoute patternRoute(nets[netIndex], *gridGraph, parameters);
        areamtx.lock();
        patternRoute.constructSteinerTree();
        areamtx.unlock();
        // if(SteinerTreeNode::getPythonString(patternRoute.getSteinerTree())!="[]"){
        //     cfile <<SteinerTreeNode::getPythonString(patternRoute.getSteinerTree())<<","<<"\n";
        // }
        // patternRoute.constructRoutingDAG();
        PatternRoutes.insert(std::make_pair(netIndex, patternRoute));
    }
    //cfile << "]";
    //cfile.close();
    double t_cst = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t_cst_b).count();

    auto t_pr_b = std::chrono::high_resolution_clock::now();
    for (const vector<int> &batch : batches)
    {
        runJobsMT(batch.size(), numofThreads, [&](int jobIdx)
                  {
            auto patternRoute = PatternRoutes.find(batch[jobIdx])->second;
            patternRoute.constructRoutingDAG();
            patternRoute.run();
            gridGraph->commitTree(nets[batch[jobIdx]].getRoutingTree()); });
    }
    // gridGraph->clearDemand();
    // for (auto net:nets)
    // {
    //     gridGraph->commitTree(net.getRoutingTree());
    // }
    
    double t_pr = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t_pr_b).count();

    // netIndices.clear();
    // for (const auto &net : nets)
    // {
    //     int netOverflow = gridGraph->checkOverflow(net.getRoutingTree());
    //     if (netOverflow > 0)
    //     {
    //         netIndices.push_back(net.getIndex());
    //         netOverflows[net.getIndex()] = netOverflow;
    //     }
    // }
    // std::cout<<"before:"<<netIndices.size()<<std::endl;

    // sortNetIndices(netIndices);
    // for (const int netIndex : netIndices){
    //     GRNet &net = nets[netIndex];
    //     gridGraph->commitTree(net.getRoutingTree(), true);
    //     //auto patternRoute = PatternRoutes.find(netIndex)->second;
    //     PatternRoute patternRoute(net, gridGraph, parameters);
    //     patternRoute.constructSteinerTree();
    //     patternRoute.constructRoutingDAG();
    //     patternRoute.run();
    //     gridGraph->commitTree(net.getRoutingTree());
    // }

    netIndices.clear();
    for (const auto &net : nets)
    {
        int netOverflow = gridGraph->checkOverflow(net.getRoutingTree());
        if (netOverflow > 0)
        {
            netIndices.push_back(net.getIndex());
            netOverflows[net.getIndex()] = netOverflow;
        }
    }
    // std::cout<<"after:"<<netIndices.size()<<std::endl;
    log() << netIndices.size() << " / " << nets.size() << " nets have overflows." << std::endl;
    logeol();

    afile << netIndices.size() << " / " << nets.size() << " nets have overflows." << std::endl;
    // afile<<netIndices.size()<<" ";
    // printStatistics();
    t1 = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t).count();
    t = std::chrono::high_resolution_clock::now();

    // Stage 2: Pattern routing with possible detours
    n2 = netIndices.size();
    if (netIndices.size() > 0)
    {
        log() << "stage 2: pattern routing with possible detours" << std::endl;
        GridGraphView<bool> congestionView; // (2d) direction -> x -> y -> has overflow?
        gridGraph->extractCongestionView(congestionView);
        // for (const int netIndex : netIndices) {
        //     GRNet& net = nets[netIndex];
        //     gridGraph->commitTree(net.getRoutingTree(), true);
        // }
#ifndef ENABLE_ISSSORT
        sortNetIndices(netIndices);
#else
        log() << "sort net indices with OFDALD" << std::endl;
        sortNetIndicesOFDALD(netIndices, netOverflows);
        // sortNetIndicesD(netIndices);
#endif
        // for (const int netIndex : netIndices)
        // {
        //     GRNet &net = nets[netIndex];
        //     gridGraph->commitTree(net.getRoutingTree(), true);
        //     PatternRoute patternRoute(net, gridGraph, parameters);
        //     patternRoute.constructSteinerTree();
        //     patternRoute.constructRoutingDAG();
        //     patternRoute.constructDetours(congestionView); // KEY DIFFERENCE compared to stage 1
        //     patternRoute.run();
        //     gridGraph->commitTree(net.getRoutingTree());
        // }

        routers.clear();
        batches.clear();
        PatternRoutes.clear();
    
        // log()<<"routers size:"<<routers.size()<<std::endl;
        // log()<<"batches size:"<<batches.size()<<std::endl;
        // log()<<"PatternRoutes size:"<<PatternRoutes.size()<<std::endl;
        routers.reserve(netIndices.size());
        for (auto id : netIndices)
            routers.emplace_back(nets[id]);
        batches = getBatches(routers, netIndices);

        // std::ofstream  aafile;
        // aafile.open("batch", std::ios::app);
        
        // for (auto batch: batches)
        // {
        //     for (auto id: batch)
        //     {
        //         aafile<<nets[id].getBoundingBox()<<" ";
        //     }
        //     aafile<<"\n";
        // }
        for (const int netIndex : netIndices)
        {
            GRNet &net = nets[netIndex];
            // gridGraph->commitTree(net.getRoutingTree(), true);
            PatternRoute patternRoute(net, *gridGraph, parameters);
            areamtx.lock();
            patternRoute.constructSteinerTree();
            areamtx.unlock();
            // patternRoute.constructRoutingDAG();
            // patternRoute.constructDetours(congestionView); // KEY DIFFERENCE compared to stage 1
            PatternRoutes.insert(std::make_pair(netIndex, patternRoute));   
        }

        //std::mutex mtx;
        for (const vector<int> &batch : batches)
        {
            runJobsMT(batch.size(), numofThreads, [&](int jobIdx)
                      {
            GRNet &net = nets[batch[jobIdx]];
            gridGraph->commitTree(net.getRoutingTree(), true);
            auto patternRoute = PatternRoutes.find(batch[jobIdx])->second;
            patternRoute.constructRoutingDAG();
            patternRoute.constructDetours(congestionView); // KEY DIFFERENCE compared to stage 1
            patternRoute.run();
            //mtx.lock();
            gridGraph->commitTree(net.getRoutingTree()); 
            //mtx.unlock();
            });
        }

        // std::vector<std::vector<std::vector<GraphEdge>>> old_demand;
        // std::vector<std::vector<std::vector<GraphEdge>>> new_demand;
        // std::vector<double> result;
        // std::ofstream  dfile;
        // dfile.open("demand", std::ios::app);
        // old_demand = gridGraph->get(); 
        // gridGraph->clearDemand();
        // for (auto net:nets)
        // {
        //     gridGraph->commitTree(net.getRoutingTree());
        // }
        // new_demand = gridGraph->get();
        // int count1 = 0;
        // for (size_t i = 0; i < old_demand.size(); i++)
        // {
        //     for (size_t j = 0; j < old_demand[i].size(); j++)
        //     {
        //         for (size_t k = 0; k < old_demand[i][j].size(); k++)
        //         {
        //             result.emplace_back(new_demand[i][j][k].demand - old_demand[i][j][k].demand);
        //             count1 ++;
        //         }
        //     }
        // }
        
        // dfile<<"total_demand_num:"<<count1<<std::endl;
        // double count2=0;
        // int total_num=0;
        // for (auto i:result)
        // {
        //     total_num += i;
        //     // if (i > 0)
        //     // {
        //     //     total_num += i;
        //     if(i !=0) 
        //     {dfile<<i<<" ";
        //     count2 ++;}
        //     // }else if (i < 0)
        //     // {
        //     //     std::cout<<"negative"<<"\n";
        //     // }
        // }
        // dfile<<std::endl;
        // dfile<<"wrong_demand_num:"<<count2<<std::endl;
        // dfile<<"average_wrong_value"<<total_num/count2<<std::endl;
        // std::cout<<"num"<<std::endl;
        // dfile.close();
        // std::cin.get();

        // gridGraph->clearDemand();
        // for (auto net : nets)
        // {
        //     gridGraph->commitTree(net.getRoutingTree());
        // }

        routers.clear();
        batches.clear();
        PatternRoutes.clear();

        netIndices.clear();
        for (const auto &net : nets)
        {
            int netOverflow = gridGraph->checkOverflow(net.getRoutingTree());
            if (netOverflow > 0)
            {
                netIndices.push_back(net.getIndex());
                netOverflows[net.getIndex()] = netOverflow;
                // log() << "netindex: " << net.getIndex() << " netoverflow: " << netOverflow << std::endl;
            }
        }

        log() << netIndices.size() << " / " << nets.size() << " nets have overflows." << std::endl;
        logeol();
        afile << netIndices.size() << " / " << nets.size() << " nets have overflows." << std::endl;
    }
    // printStatistics();
    t2 = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t).count();
    t = std::chrono::high_resolution_clock::now();

    // Stage 3: maze routing
    n3 = netIndices.size();
#ifndef ENABLE_CUDA
    if (netIndices.size() > 0)
    {
        log() << "stage 3: maze routing on sparsified routing graph" << std::endl;
        for (const int netIndex : netIndices)
        {
            GRNet &net = nets[netIndex];
            gridGraph->commitTree(net.getRoutingTree(), true);
        }
        GridGraphView<CostT> wireCostView;
        gridGraph->extractWireCostView(wireCostView);
#ifndef ENABLE_ISSSORT
        sortNetIndices(netIndices);
#else
        log() << "sort net indices with OFDALD" << std::endl;
        sortNetIndicesOFDALD(netIndices, netOverflows);
#endif
        SparseGrid grid(10, 10, 0, 0);
        for (const int netIndex : netIndices)
        {
            GRNet &net = nets[netIndex];
            // gridGraph->commitTree(net.getRoutingTree(), true);
            // gridGraph->updateWireCostView(wireCostView, net.getRoutingTree());
            MazeRoute mazeRoute(net, *gridGraph, parameters);
            mazeRoute.constructSparsifiedGraph(wireCostView, grid);
            mazeRoute.run();
            std::shared_ptr<SteinerTreeNode> tree = mazeRoute.getSteinerTree();
            assert(tree != nullptr);

            PatternRoute patternRoute(net, *gridGraph, parameters);
            patternRoute.setSteinerTree(tree);
            patternRoute.constructRoutingDAG();
            patternRoute.run();

            gridGraph->commitTree(net.getRoutingTree());
            gridGraph->updateWireCostView(wireCostView, net.getRoutingTree());
            grid.step();
        }
        netIndices.clear();
        for (const auto &net : nets)
        {
            if (gridGraph->checkOverflow(net.getRoutingTree()) > 0)
            {
                netIndices.push_back(net.getIndex());
            }
        }
        log() << netIndices.size() << " / " << nets.size() << " nets have overflows." << std::endl;
        logeol();
        afile << netIndices.size() << " / " << nets.size() << " nets have overflows." << std::endl;
    }
#else
    if (netIndices.size() > 0)
    {
        log() << "stage 3: gpu maze routing\n";
        GPUMazeRoute mazeRoute(nets, gridGraph, parameters);
        mazeRoute.run();
        mazeRoute.getOverflowNetIndices(netIndices);
        log() << netIndices.size() << " / " << nets.size() << " nets have overflows." << std::endl;
    }
#endif
    t3 = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t).count();
    t = std::chrono::high_resolution_clock::now();

    log() << "step routed #nets: " << n1 << ", " << n2 << ", " << n3 << "\n";
    log() << "step time consumption: "
          << std::setprecision(3) << std::fixed << t1 << " s, "
          << std::setprecision(3) << std::fixed << t2 << " s, "
          << std::setprecision(3) << std::fixed << t3 << " s\n";
    
    afile<< t_batch << " " << t_cst << " " << t_pr << " ";
    afile<< t1 << " " << t2 << " " << t3 << " ";
    afile.close();
    // printStatistics();
    // if (parameters.write_heatmap)
    //     gridGraph->write();
}

void GlobalRouter::write(std::string guide_file, std::unordered_map<std::string, std::vector<GRNet>> outGRNet, std::vector<std::pair<std::string,std::tuple<int, int, int>>> pin1net) {
    std::vector<int> gcell_x;
    std::vector<int> gcell_y;
    
    int temp_x = 0;
    for (auto& x : horizontal_gcell_edge_lengths)
    {
        gcell_x.push_back(x/2 + temp_x);
        temp_x += x;
    }
    int temp_y = 0;
    for (auto& y : vertical_gcell_edge_lengths)
    {
        gcell_y.push_back(y/2 + temp_y);
        temp_y += y;
    }

    log() << "generating route guides..." << std::endl;
    if (guide_file == "")
        guide_file = parameters.out_file;

    areaOfPinPatches = 0;
    areaOfWirePatches = 0;
    std::stringstream ss;
    std::vector<std::array<int, 6>> guide;

    for (auto nets : outGRNet)
    {
        ss << nets.first << "\n";
        ss << "(\n";
        for (const GRNet &net : nets.second)
        {
            getGuide(net, guide);
            // ss << net.getName() << std::endl;
            for (const auto &[lx, ly, lz, hx, hy, hz] : guide){

                // ss << (lx)*4200 + 2100 << " " << (ly)*4200 + 2100 << " " << "M" <<lz+1 << " " << (hx)*4200 + 2100 << " " << (hy)*4200 + 2100 << " " << "M" << hz+1 << "\n";
                // ss << lx << " " << ly << " " << "M" <<lz+1 << " " << hx << " " << hy << " " << "M" << hz+1 << "\n";
                if (lz != hz)
                {
                    // std::cout<<nets.first<<std::endl;
                    for (size_t i = lz; i < hz; i++)
                    {
                        ss << gcell_x[lx] << " " << gcell_y[ly] << " " << "metal" <<i+1 << " " << gcell_x[hx] << " " << gcell_y[hy] << " " << "metal" << i+2 << "\n";
                    }
                }else
                {
                    ss << gcell_x[lx] << " " << gcell_y[ly] << " " << "metal" <<lz+1 << " " << gcell_x[hx] << " " << gcell_y[hy] << " " << "metal" << hz+1 << "\n";
                }
            }
        }
        ss << ")\n";
    }
    
    // for (auto& net : pin1net)
    // {
    //     ss << net.first <<"\n";
    //     ss << "(\n";
    //     auto pin = net.second;
    //     ss << gcell_x[std::get<0>(pin)] << " " << gcell_x[std::get<1>(pin)] <<" "<<"metal"<< std::get<2>(pin)+1<<" "<<gcell_x[std::get<0>(pin)] << " " << gcell_x[std::get<1>(pin)] <<" "<<"metal"<< std::get<2>(pin)+1<<"\n";
    //     ss << ")\n";
    // }
    
    // for (auto& pin0 : pin0net)
    // {
    //     ss << pin0 << std::endl;
    //     ss << "(\n";
    //     ss << ")\n";
    // }

    log() << std::endl;
    log() << "writing output..." << std::endl;
    std::ofstream fout(guide_file);
    fout << ss.str();
    fout.close();
    log() << "finished writing output..." << std::endl;

    std::cout<<pin0net.size() + pin1net.size() + outGRNet.size()<<std::endl;
    std::cout<<pin0net.size()<<" "<<pin1net.size() <<" "<< outGRNet.size()<<std::endl;
}

void GlobalRouter::getGuide(const GRNet &net, std::vector<std::array<int, 6>> &guide)
{
    guide.clear();
    auto tree = net.getRoutingTree();
    if(tree == nullptr)
        return;
    else if(tree->children.size() == 0){
        int layer1 = min(tree->layerIdx, static_cast<int>(gridGraph->getNumLayers()-1));
        int layer2 = min(tree->layerIdx+1, static_cast<int>(gridGraph->getNumLayers()));
        guide.push_back({ tree->x, tree->y, layer1, tree->x, tree->y, layer2  });
    }
    else
        GRTreeNode::preorder(tree, [&](std::shared_ptr<GRTreeNode> node) {
            for(const auto &child : node->children) {
                if(node->layerIdx == child->layerIdx && node->x == child->x && node->y == child->y)
                    continue;
                else
                guide.push_back({ 
                    std::min(node->x, child->x),
                    std::min(node->y, child->y),
                    std::min(node->layerIdx, child->layerIdx),
                    std::max(node->x, child->x),
                    std::max(node->y, child->y),
                    std::max(node->layerIdx, child->layerIdx),
                });
            }
        });
}