#pragma once

#include <vector>
#include <unordered_map>
#include <cmath>

#include "sql.h"
#include "utils.h"

using GraphEdgeIndex = int;
using GraphNodeIndex = int;

struct GraphEdge
{
    long long int sqlID;
    GraphNodeIndex to;
    int weight;
    bool isPrimary;
};

struct GraphNode
{
    sql::Node data;
    std::vector<GraphEdgeIndex> outEdges;
};

class MapGraph
{
public:
    /**
     * Loads graph data from the specified database path and initializes internal graph.
     *
     * Populates nodes and edges from the database into the graph. It updates chunked node storage
     * and maps SQL node IDs to graph node indices. Edges are also loaded and linked to the appropriate nodes.
     * This method is idempotent.
     *
     * @param dbPath The path to the database file from which to load graph data.
     */
    void load(std::string dbPath)
    {
        if (isLoaded)
            return;

        auto storage = sql::loadStorage(dbPath);

        // load all nodes from the db into graph
        for (sql::Node node : storage.iterate<sql::Node>())
        {
            nodes.push_back(GraphNode{node, {}});
            nodeSQLIdToNodeIndex.emplace(node.id, nodes.size() - 1);

            int chunkRow = std::stoi(splitString(node.chunkId, ",")[0]);
            int chunkCol = std::stoi(splitString(node.chunkId, ",")[1]);

            if (chunkedGraphNodes.size() <= chunkRow)
                chunkedGraphNodes.resize(chunkRow + 1);

            if (chunkedGraphNodes[chunkRow].size() <= chunkCol)
                chunkedGraphNodes[chunkRow].resize(chunkCol + 1);

            chunkedGraphNodes[chunkRow][chunkCol].push_back(nodes.size() - 1);
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

        isLoaded = true;
    }

    /**
     * Finds the index of the node that is nearest to a given point specified by longitude and latitude offsets.
     *
     * This method searches through the nodes in a given chunk and find the node with the shortest straight
     * line distance to the given point.
     *
     * @param chunkRow The row index of the chunk in which to search for nodes.
     * @param chunkCol The column index of the chunk in which to search for nodes.
     * @param offsetLongitude The offset longitude of the point.
     * @param offsetLatitude The offset latitude of the point.
     *
     * @return The index of the nearest node within the specified chunk
     */
    GraphNodeIndex findNearestNode(int chunkRow, int chunkCol, double offsetLongitude, double offsetLatitude)
    {
        using std::pow;
        using std::sqrt;

        double x0 = offsetLongitude;
        double y0 = offsetLatitude;
        double smallestDistance = 100000000; // no distance will be larger than this;
        GraphEdgeIndex closestNodeIndex = -1;

        for (const GraphNodeIndex &idx : chunkedGraphNodes.at(chunkRow).at(chunkCol))
        {
            const GraphNode &node = nodes.at(idx);
            double x1 = node.data.offsetLon;
            double y1 = node.data.offsetLat;

            // euclidean distance with pythagorean theorem
            double distance = sqrt(pow(x1 - x0, 2) + pow(y1 - y0, 2));

            if (distance < smallestDistance)
            {
                smallestDistance = distance;
                closestNodeIndex = idx;
            }
        }

        return closestNodeIndex;
    }

    bool isDataLoaded() const
    {
        return isLoaded;
    }

    GraphNode &getNode(GraphNodeIndex nodeIndex)
    {
        return nodes[nodeIndex];
    }

    GraphEdge &getEdge(GraphEdgeIndex edgeIndex)
    {
        return edges[edgeIndex];
    }

    int getNodeCount()
    {
        return nodes.size();
    }

    int getEdgeCount()
    {
        return edges.size();
    }

private:
    std::unordered_map<long long int, int> nodeSQLIdToNodeIndex;
    std::vector<GraphNode> nodes;
    std::vector<GraphEdge> edges;
    bool isLoaded = false;

    std::vector<std::vector<std::vector<GraphNodeIndex>>> chunkedGraphNodes;
};