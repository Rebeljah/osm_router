#pragma once

#include <vector>
#include <unordered_map>

#include "node.h"
#include "sql.h"

using GraphEdgeID = int;
using GraphNodeID = int;

struct GraphEdge
{
    long long int sqlID;
    GraphNodeID to;
    int weight;
    bool isPrimary;
};

struct GraphNode
{
    long long int sqlID;
    std::vector<GraphEdgeID> outEdges;
};

class MapGraph
{
public:
    void load(std::string dbPath)
    {
        auto storage = sql::loadStorage(dbPath);

        // load all nodes from the db into graph
        for (sql::Node node : storage.iterate<sql::Node>())
        {
            nodes.push_back(GraphNode{node.id, {}});
            nodeSQLIdToNodeIndex.emplace(node.id, nodes.size() - 1);
        }
        
        // load and insert all edges
        for (sql::Edge edge : storage.iterate<sql::Edge>())
        {
            // TODO improve heuristic
            int weight = int(edge.pathLengthMeters);

            int idxSourceNode = nodeSQLIdToNodeIndex.at(edge.sourceNodeId);
            int idxTargetNode = nodeSQLIdToNodeIndex.at(edge.targetNodeId);

            GraphNode &sourceNode = nodes.at(idxSourceNode);
            GraphNode &targetNode = nodes.at(idxTargetNode);

            if ((PathDescriptor)edge.pathCarFwd != PathDescriptor::Forbidden)
            {
                edges.push_back(GraphEdge{edge.id, idxTargetNode, weight, true});
                sourceNode.outEdges.push_back(edges.size() - 1);
            }

            if ((PathDescriptor)edge.pathCarBwd != PathDescriptor::Forbidden)
            {
                edges.push_back(GraphEdge{edge.id, idxSourceNode, weight, false});
                targetNode.outEdges.push_back(edges.size() - 1);
            }
        }
    }
private:
    std::unordered_map<long long int, int> nodeSQLIdToNodeIndex;
    std::vector<GraphNode> nodes;
    std::vector<GraphEdge> edges;
};