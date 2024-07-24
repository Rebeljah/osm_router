#include <iostream>
#include <SFML/Graphics.hpp>
#include "app.h"
#include <utility>
#include <queue>
#include <set>
#include <limits>

using namespace std;

/**
 * Finds the shortest path between two nodes using Dijkstra's algorithm.
 * 
 * @param startNodeIndex The index of the start node
 * @param endNodeIndex The index of the end node
 * @param graph The graph to search
 * @return The shortest path between the two nodes
*/
vector<GraphNodeIndex> Dijkstra(GraphNodeIndex startNodeIndex, GraphNodeIndex endNodeIndex, MapGraph& graph) {
    // TODO Consider visited set to avoid revisiting nodes.

    // TODO Could a hash table be better for the average travel distance?
    vector<long long int> weights(graph.getNodeCount(), 9999999999999);
    weights[startNodeIndex] = 0;    // The distance from the start node to itself is 0.

    // Each index in prev is a node that holds its previous node prev[D] = A if the shortest path so far to D came from A.
    vector<GraphNodeIndex> prev(graph.getNodeCount(), -1);

    // pq.first = distance to start node, pq.second = the nodeID; 
    priority_queue<pair<int, GraphNodeIndex>, vector<pair<int, GraphNodeIndex>>, greater<pair<int, GraphNodeIndex>>> minPQ;
    minPQ.push(make_pair(0, startNodeIndex));

    while (!minPQ.empty()) {
        std::pair<int, GraphNodeIndex> v = minPQ.top(); 
        minPQ.pop();
        GraphNode currentNode = graph.getNode(v.second);
        for (auto edgeIndex : currentNode.outEdges) {
            GraphEdge edge = graph.getEdge(edgeIndex);
            long long int distanceFromStart = edge.weight + weights[v.second];
            if (weights[edge.to] > distanceFromStart) {
                weights[edge.to] = distanceFromStart;
                prev[edge.to] = v.second;
                GraphNodeIndex targetNodeIndex = edge.to;
                minPQ.push(make_pair(distanceFromStart, targetNodeIndex));
            }
        }

        // Stop early if we have reached the end node to avoid unnecessary computation.
        // This doesn't guarantee the shortest path because not all nodes have been visited necessarily.
        // TODO Remove this if we want to guarantee the shortest path at the cost of more computation.
        if (v.second == endNodeIndex) {
             vector<GraphNodeIndex> path;
             GraphNodeIndex current = endNodeIndex;
             while (current != -1) {
                path.push_back(current);
                current = prev[current];
             }
             return path;
        }

    }

    return vector<GraphNodeIndex>();    // Empty vector if no path exists.
}

/**
 * Finds a shortest path between origin and destination using the selected algorithm
 * 
 * @return The shortest path
*/
vector<GraphNodeIndex> findShortestPath(sf::Vector2<double> offsetLonLatOrigin, sf::Vector2<double> offsetLonLatDestination, AlgoName algorithm, MapGraph& mapGraph, MapGeometry& mapGeometry) {
    // Get the origin and destination nodes
    pair<int, int> startChunkCoordinate = mapGeometry.getChunkRowCol(offsetLonLatOrigin.y, offsetLonLatOrigin.x);
    pair<int, int> endChunkCoordinate = mapGeometry.getChunkRowCol(offsetLonLatDestination.y, offsetLonLatDestination.x);
    GraphNodeIndex startNodeIndex = mapGraph.findNearestNode(startChunkCoordinate.first, startChunkCoordinate.second, offsetLonLatOrigin.x, offsetLonLatOrigin.y);
    GraphNodeIndex endNodeIndex = mapGraph.findNearestNode(endChunkCoordinate.first, endChunkCoordinate.second, offsetLonLatDestination.x, offsetLonLatDestination.y);
    GraphNode startNode = mapGraph.getNode(startNodeIndex);
    GraphNode endNode = mapGraph.getNode(endNodeIndex);

    if (algorithm == AlgoName::Dijkstras) {
        return Dijkstra(startNodeIndex, endNodeIndex, mapGraph);
    }
    else {
        // TODO: Perform A* search
    }
}
