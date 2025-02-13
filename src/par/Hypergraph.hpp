#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>   // std::accumulate
float balance = 0.25;
float left = -0.5;
float bottom = -0.5;
float size = 1.0;
int rows = 50;
int cols = 50;
float gridSize = size / rows; // 每个小方格的边长
std::vector<std::pair<float,float>> nomalizePlacementDat;

// Forward declaration of the Hyperedge class
class Hyperedge;

class Node {
public:
    int idx;
    int weight;
    int par;
    std::vector<Hyperedge*> hyperedges;
    std::pair<float, float> position;

    Node(const int idx, int w, std::pair<float, float> pos, int par);

    void addHyperedge(Hyperedge* edge);
    void removeHyperedge(Hyperedge* edge);
    void printInfo() const;
};

class Hyperedge {
public:
    int idx;
    float weight;
    std::vector<Node*> nodes;
    std::set<float>xset;
    std::set<float>yset;
    float slack;
    std::pair<std::pair<float, float>, std::pair<float, float>> bbox;
    int par;

    Hyperedge(const int idx, float w, int p);

    void addNode(Node* node);
    void removeNode(Node* node);
    void printInfo() const;
    std::pair<std::pair<float, float>, std::pair<float, float>> getBoundingBox();
};


class Hypergraph {
private:
    std::unordered_map<int, std::shared_ptr<Node>> nodes;
    std::unordered_map<int, std::shared_ptr<Hyperedge>> edges;

public:
    float sum_e_weight;
    void addNode(const int idx, int weight, std::pair<float, float> position, int par);
    void removeNode(const int idx);
    void addHyperedge(const int idx, float weight, int p);
    void removeHyperedge(const int idx);
    void connectNodeToHyperedge(const int nodeidx, const int edgeidx);
    void disconnectNodeFromHyperedge(const int nodeidx, const int edgeidx);
    int getNodeCount() const;
    int getHyperedgeCount() const;
    void printNodes() const;
    void printHyperedges() const;
    void loadFromTopoFile_uw(std::unordered_map<int, std::vector<int>> hyperEdges, int nodeCount);
    void loadFromPlacementFile_uw();
    void NomalizePlacementDat(std::vector<std::pair<float, float>> Coor);
    std::vector<int> Generate_output(Hypergraph& hg);
    std::unordered_map<int, std::shared_ptr<Node>>& getNodes()  {
        return nodes;
    }

    std::unordered_map<int, std::shared_ptr<Hyperedge>>& getEdges()  {
        return edges;
    }
    void cross_cut(Hypergraph& hg);
    std::pair<float, float> evaluate_cut(std::vector<std::pair<float, float>> leftvec,
        std::vector<std::pair<float, float>> rightvec,
        float x_cut, float We_sum);
    void Initialize_vec(Hypergraph& hg,
        std::vector<std::pair<float, float>>& leftedge_vec,
        std::vector<std::pair<float, float>>& rightedge_vec,
        std::vector<std::pair<float, float>>& topedge_vec,
        std::vector<std::pair<float, float>>& bottomedge_vec);
    void initial_bbox(Hypergraph& hg);
};

// Node class implementation
Node::Node(const int idx, int w, std::pair<float, float> pos, int par)
    : idx(idx), weight(w), position(pos), par(par) {}

void Node::addHyperedge(Hyperedge* edge) {
    hyperedges.push_back(edge);
}

void Node::removeHyperedge(Hyperedge* edge) {
    hyperedges.erase(std::remove(hyperedges.begin(), hyperedges.end(), edge), hyperedges.end());
}

void Node::printInfo() const {
    std::cout << "Node Name: " << idx
        << ", Weight: " << weight
        << ", Position: (" << position.first << ", " << position.second << ")\n";
}

// Hyperedge class implementation
Hyperedge::Hyperedge(const int idx, float w, int p)
    : idx(idx), weight(w), par(p){}

void Hyperedge::addNode(Node* node) {
    nodes.push_back(node);
    node->addHyperedge(this);
}

void Hyperedge::removeNode(Node* node) {
    nodes.erase(std::remove(nodes.begin(), nodes.end(), node), nodes.end());
    node->removeHyperedge(this);
}

void Hyperedge::printInfo() const {
    std::cout << "Hyperedge Name: " << idx
        << ", Weight: " << weight << "\n";
    std::cout << "Connected Nodes: ";
    for (const auto& node : nodes) {
        std::cout << node->idx << " ";
    }
    std::cout << "\n";
}

std::pair<std::pair<float, float>, std::pair<float, float>> Hyperedge::getBoundingBox() {
    if (xset.empty() || yset.empty()) {
        std::cerr << "点集为空，无法计算最小矩形框！";
    }

    // 左下角和右上角坐标
    float minX = *xset.begin();  // x最小值
    float maxX = *xset.rbegin(); // x最大值
    float minY = *yset.begin();  // y最小值
    float maxY = *yset.rbegin(); // y最大值

    return std::make_pair(std::make_pair(minX, minY), std::make_pair(maxX, maxY));
};



//Hypergraph class implementation
void Hypergraph::addNode(const int idx, int weight, std::pair<float, float> position, int par) {
    if (nodes.find(idx) == nodes.end()) {
        nodes[idx] = std::make_shared<Node>(idx, weight, position, par);
    }
    else {
        std::cerr << "Node " << idx << " already exists.\n";
    }
}

void Hypergraph::removeNode(const int idx) {
    if (nodes.find(idx) != nodes.end()) {
        Node* node = nodes[idx].get();
        for (auto* edge : node->hyperedges) {
            edge->removeNode(node);
        }
        nodes.erase(idx);
    }
    else {
        std::cerr << "Node " << idx << " not found.\n";
    }
}

void Hypergraph::addHyperedge(const int idx, float weight, int par) {
    if (edges.find(idx) == edges.end()) {
        edges[idx] = std::make_shared<Hyperedge>(idx, weight, par);
    }
    else {
        std::cerr << "Hyperedge " << idx << " already exists.\n";
    }
}

void Hypergraph::removeHyperedge(const int idx) {
    if (edges.find(idx) != edges.end()) {
        Hyperedge* edge = edges[idx].get();
        for (auto* node : edge->nodes) {
            node->removeHyperedge(edge);
        }
        edges.erase(idx);
    }
    else {
        std::cerr << "Hyperedge " << idx << " not found.\n";
    }
}

void Hypergraph::connectNodeToHyperedge(const int nodeidx, const int edgeidx) {
    if (nodes.find(nodeidx) != nodes.end() && edges.find(edgeidx) != edges.end()) {
        edges[edgeidx]->addNode(nodes[nodeidx].get());
    }
    else {
        std::cerr << "Node or hyperedge not found.\n";
    }
}

void Hypergraph::disconnectNodeFromHyperedge(const int nodeidx, const int edgeidx) {
    if (nodes.find(nodeidx) != nodes.end() && edges.find(edgeidx) != edges.end()) {
        edges[edgeidx]->removeNode(nodes[nodeidx].get());
    }
    else {
        std::cerr << "Node or hyperedge not found.\n";
    }
}

int Hypergraph::getNodeCount() const {
    return nodes.size();
}

int Hypergraph::getHyperedgeCount() const {
    return edges.size();
}

void Hypergraph::printNodes() const {
    std::cout << "Nodes in Hypergraph:\n";
    for (const auto& pair : nodes) {
        pair.second->printInfo();
    }
}

void Hypergraph::printHyperedges() const {
    std::cout << "Hyperedges in Hypergraph:\n";
    for (const auto& pair : edges) {
        pair.second->printInfo();
        std::cout << "\n" << std::endl;
    }
}
void Hypergraph::NomalizePlacementDat(std::vector<std::pair<float, float>> Coor) {
    std::vector<float> X;
    std::vector<float> Y;
    for (auto& coor : Coor)
    {
        X.push_back(coor.first);
        Y.push_back(coor.second);
    }

    std::sort(X.begin(), X.end(), [](float a, float b) {
        return a > b;
        });
    std::sort(Y.begin(), Y.end(), [](float a, float b) {
        return a > b;
        });
    float x_max = X[0];
    float x_min = X[X.size() - 1];
    float y_max = Y[0];
    float y_min = Y[Y.size() - 1];
    float range_x, range_y;
    range_x = x_max - x_min;
    range_y = y_max - y_min;
    for (const auto& coor : Coor)
    {
        float x = coor.first;
        float y = coor.second;
        float nm_x, nm_y;
        nm_x = (x - x_min) / range_x - 0.5;
        nm_y = (y - y_min) / range_y - 0.5;
        nomalizePlacementDat.emplace_back(nm_x, nm_y);
    }
}

//---------------------------------------------------------------------------------------CreatGrid----------------------------------------------------------------------------------------------------------------------

void initial_bbox(Hypergraph& hg) {
	for (auto& net : hg.getEdges()) {
		net.second->bbox = net.second->getBoundingBox();
	}
}

void Initialize_vec(Hypergraph& hg,
	std::vector<std::pair<float, float>>& leftedge_vec,
	std::vector<std::pair<float, float>>& rightedge_vec,
	std::vector<std::pair<float, float>>& topedge_vec,
	std::vector<std::pair<float, float>>& bottomedge_vec) {
	for (auto& net : hg.getEdges()) {
		std::pair<std::pair<float, float>, std::pair<float, float>> bbox = net.second->bbox;
		int pin_num = net.second->nodes.size();
		
		net.second->weight = pin_num * (bbox.second.first - bbox.first.first) * 
			(bbox.second.second - bbox.first.second);
		
		
		hg.sum_e_weight += net.second->weight;
		leftedge_vec.push_back(std::make_pair(bbox.first.first, net.second->weight));
		rightedge_vec.push_back(std::make_pair(bbox.second.first, net.second->weight));
		topedge_vec.push_back(std::make_pair(bbox.second.second, net.second->weight));
		bottomedge_vec.push_back(std::make_pair(bbox.first.second, net.second->weight));
	}
	std::sort(leftedge_vec.begin(), leftedge_vec.end(), [](const std::pair<float, float>& a,
		const std::pair<float, float>& b) {
			return a.first < b.first; // 只比较第一个 float
		});
	std::sort(rightedge_vec.begin(), rightedge_vec.end(), [](const std::pair<float, float>& a,
		const std::pair<float, float>& b) {
			return a.first < b.first; // 只比较第一个 float
		});
	std::sort(bottomedge_vec.begin(), bottomedge_vec.end(), [](const std::pair<float, float>& a,
		const std::pair<float, float>& b) {
			return a.first < b.first; // 只比较第一个 float
		});
	std::sort(topedge_vec.begin(), topedge_vec.end(), [](const std::pair<float, float>& a,
		const std::pair<float, float>& b) {
			return a.first < b.first; // 只比较第一个 float
		});
	//std::cout << "stage3: Initial_vec done" << std::endl;
}



//---------------------------------------------------------------evaluation--------------------------------------------------------------------------------

std::pair<float, float> evaluate_cut(std::vector<std::pair<float, float>> leftvec,
	std::vector<std::pair<float, float>> rightvec,
	float x_cut, float We_sum) {
	if (x_cut >= leftvec.rbegin()->first || x_cut <= leftvec.begin()->first) {
		return std::make_pair(0, We_sum);
	}
	float left_weight, right_weight, cut_size;
	auto idx1 = std::upper_bound(rightvec.begin(), rightvec.end(), std::make_pair(x_cut, 0.0f));
	left_weight = std::accumulate(rightvec.begin(), idx1, 0.0, [](float acc, const std::pair<float, float>& p) {
		return acc + p.second;
		});
	auto idx2 = std::lower_bound(leftvec.begin(), leftvec.end(), std::make_pair(x_cut, 0.0f));
	if (idx2->first == x_cut) {
		right_weight = std::accumulate(idx2, leftvec.end(), 0.0, [](float acc, const std::pair<float, float>& p) {
			return acc + p.second;
			});
	}
	else {
		auto idx3 = std::upper_bound(leftvec.begin(), leftvec.end(), std::make_pair(x_cut, 0.0f));
		right_weight = std::accumulate(idx3, leftvec.end(), 0.0, [](float acc, const std::pair<float, float>& p) {
			return acc + p.second;
			});
	}
	cut_size = We_sum - (left_weight + right_weight);
	float balance_constraint = std::abs(left_weight - right_weight);
	return std::make_pair(cut_size, balance_constraint);
}


//----------------------------------------------------------------Partition------------------------------------------------------------------------
void cross_cut(Hypergraph& hg) {
	std::vector<std::pair<float, float>> leftedge_vec;
	std::vector<std::pair<float, float>> rightedge_vec;
	std::vector<std::pair<float, float>> topedge_vec;
	std::vector<std::pair<float, float>> bottomedge_vec;
	initial_bbox(hg);
	auto start0 = std::chrono::high_resolution_clock::now();
	Initialize_vec(hg, leftedge_vec, rightedge_vec, topedge_vec, bottomedge_vec); 
	auto end0 = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> duration0 = end0 - start0;
	std::cout << "Initial time taken: " << duration0.count() << " seconds\n";

	auto start1 = std::chrono::high_resolution_clock::now();
	float start_x = leftedge_vec.begin()->first;
	float start_y = bottomedge_vec.begin()->first;
	float cutsize_min_x = std::numeric_limits<float>::infinity();
	float cutsize_min_y = std::numeric_limits<float>::infinity();
	float cut_result_x = start_x;
	float cut_result_y = start_y;
	for (int i = 0; i < cols; i++) {
		float cut = start_x + i * gridSize;
		if (cut >= leftedge_vec.rbegin()->first) break;
		std::pair<float, float> eval = evaluate_cut(leftedge_vec, rightedge_vec, cut, hg.sum_e_weight);
		if (eval.second > balance * hg.sum_e_weight) continue;
		float cutsize = eval.first;
		if (cutsize < cutsize_min_x) {
			cut_result_x = cut;
			cutsize_min_x = cutsize;
		}
	}
	for (int i = 0; i < rows; i++) {
		float cut = start_y + i * gridSize;
		if (cut >= bottomedge_vec.rbegin()->first) break;
		std::pair<float, float> eval = evaluate_cut(bottomedge_vec, topedge_vec, cut, hg.sum_e_weight);
		if (eval.second > balance * hg.sum_e_weight) continue;
		float cutsize = eval.first;
		if (cutsize < cutsize_min_y) {
			cut_result_y = cut;
			cutsize_min_y = cutsize;
		}
	}

	
	auto end1 = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> duration1 = end1 - start1;
	std::cout << "Evaluate time taken: " << duration1.count() << " seconds\n";

	for (auto& node:hg.getNodes()) {
		if (node.second->position.first <= cut_result_x && node.second->position.second >= cut_result_y) node.second->par = 0;
		else if (node.second->position.first >= cut_result_x && node.second->position.first >= cut_result_y) node.second->par = 1;
		else if (node.second->position.first <= cut_result_x && node.second->position.first <= cut_result_y) node.second->par = 2;
		else if (node.second->position.first >= cut_result_x && node.second->position.first <= cut_result_y) node.second->par = 3;
	}
}

std::vector<int> Hypergraph::Generate_output(Hypergraph& hg)
{
    std::vector<int> initial_partition;
    std::vector<Node*> nodelist;
    for (auto& node : hg.getNodes())   nodelist.push_back(node.second.get());

    std::sort(nodelist.begin(), nodelist.end(), [](const Node* a, const Node* b) {
        return a->idx < b->idx;
        });
    int i = 0;
    for (i = 0; i < nodelist.size(); i++) {
        initial_partition.push_back(nodelist[i]->par);
    }
    return initial_partition;
}

void Hypergraph::loadFromTopoFile_uw(std::unordered_map<int, std::vector<int>> hyperEdges, int nodeCount){
    int hyperedgeCount = hyperEdges.size();
    for (int i = 1; i <= nodeCount; ++i) {
        int nodeidx = i;
        addNode(nodeidx, 1, { 0.0f, 0.0f }, 0);
    }
    for (int i = 1; i <= hyperedgeCount; ++i) {
        int edgeidx = i;
        addHyperedge(edgeidx, 1, 0);

        int nodeidx;
        for (auto& edge : hyperEdges[i])
        {
            nodeidx = edge;
            connectNodeToHyperedge(nodeidx, edgeidx);
        }
    }
}

 void Hypergraph::loadFromPlacementFile_uw(){
    int i = 1;
    for (const auto& dat : nomalizePlacementDat)
    {
        int nodeidx = i;
        float x = dat.first;
        float y = dat.second;
        if (std::abs(x) > 0.5 || std::abs(y) > 0.5)  std::cerr << "Error: Please input the normalized coordinate" << "\n";
        nodes[nodeidx]->position.first = x;
        nodes[nodeidx]->position.second = y;
        for (auto& net : nodes[nodeidx]->hyperedges) {
            net->xset.insert(x);
            net->yset.insert(y);
        }
        i++;
    }
}




