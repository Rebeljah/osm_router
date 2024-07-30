#include <iostream>
#include <SFML/Graphics.hpp>
#include "app.h"
#include "geometry.h"
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
vector<GraphEdgeIndex> Dijkstra(GraphNodeIndex startNodeIndex, GraphNodeIndex endNodeIndex, MapGraph& graph) {
    vector<long long int> weights(graph.getNodeCount(), 9999999999999);
    weights[startNodeIndex] = 0;    // The distance from the start node to itself is 0.

    // Each index in prev is a node that holds its previous node prev[D] = A if the shortest path so far to D came from A.
    vector<GraphNodeIndex> prev(graph.getNodeCount(), -1);
    vector<GraphEdgeIndex> pathEdges(graph.getEdgeCount(), -1);

    // pq.first = distance to start node, pq.second = the nodeID; 
    priority_queue<pair<int, GraphNodeIndex>, vector<pair<int, GraphNodeIndex>>, greater<pair<int, GraphNodeIndex>>> minPQ;
    minPQ.push(make_pair(0, startNodeIndex));

    while (!minPQ.empty()) {
        std::pair<int, GraphNodeIndex> v = minPQ.top(); 
        minPQ.pop();

        GraphNode currentNode = graph.getNode(v.second);

        for (auto edgeIndex : currentNode.outEdges) {
            GraphEdge edge = graph.getEdge(edgeIndex);
            GraphNodeIndex targetNodeIndex = edge.to;

            // Calculate the distance from the start node to the current node.
            // And update if the new distance is shorter.
            long long int distanceFromStart = edge.weight + weights[v.second];
            if (distanceFromStart < weights[targetNodeIndex]) {
                weights[targetNodeIndex] = distanceFromStart;
                prev[targetNodeIndex] = v.second;
                pathEdges[targetNodeIndex] = edgeIndex;
                minPQ.push(make_pair(distanceFromStart, targetNodeIndex));
            }
        }

        // Stop early if we have reached the end node to avoid unnecessary computation.
        // Guaranteed to be the shortest path.
        if (v.second == endNodeIndex) {
            vector<GraphEdgeIndex> path;
            vector<GraphNodeIndex> pathNodes;
            GraphNodeIndex current = endNodeIndex;
            while (current != -1) {
                pathNodes.push_back(current);
                path.push_back(pathEdges[current]);
                current = prev[current];
            }

            reverse(pathNodes.begin(), pathNodes.end());
            reverse(path.begin(), path.end());
            path.erase(path.begin());   // Since the start node has no edge leading to it, we remove the first edge which has -1.

            // TESTING: This loop prints the path of edges, and the nodes in the path.
            for (auto e : path) {
                auto edge = graph.getEdge(e);
                std::cout << "EdgeID: " << e << std::endl;
            }
            MapGeometry mapGeometry;
            for (auto e : pathNodes) {
                auto node = graph.getNode(e);
                auto globalLonLat = mapGeometry.unoffsetGeoVector({node.data.offsetLon, node.data.offsetLat});
                std::cout << "Node: " << e << " at " << globalLonLat.y << " " << globalLonLat.x << std::endl;
            }

            return path;
        }
    }

    return vector<GraphEdgeIndex>();    // Empty vector if no path exists.
}

/**
 * Finds the shortest path between two nodes using A* search.
 * 
 * @param startNodeIndex The index of the start node
 * @param endNodeIndex The index of the end node
 * @param graph The graph to search
 * @return The shortest path between the two nodes
*/
vector<GraphEdgeIndex> aStarSearch(GraphNodeIndex startNodeIndex, GraphNodeIndex endNodeIndex, MapGraph& graph) {
    /* 
    This is very similar to Djikstra's algorithm, but with a heuristic added to the weights.
    The hueuristic is the euclidean distance from the current node to the end node, 
    which defines the shortest possible distance. This means that the algorithm will prioritize 
    nodes that are closer to the end node geometrically rather than just the shortest path between the nodes.
    */

    // It's necessary to track both the total distance traveled to the node as well as each node's heuristic.
    vector<long long int> weights(graph.getNodeCount(), 9999999999999);
    weights[startNodeIndex] = 0;
    vector<long long int> heuristic(graph.getNodeCount(), 9999999999999);
    heuristic[endNodeIndex] = 0;
    
    // Initializes the heuristic for the start node.
    GraphNode startNode = graph.getNode(startNodeIndex);
    GraphNode endNode = graph.getNode(endNodeIndex);
    double endX = endNode.data.offsetLon;               
    double endY = endNode.data.offsetLat;
    double startX = startNode.data.offsetLon;
    double startY = startNode.data.offsetLat;
    heuristic[startNodeIndex] = distanceBetweenPoints(startX, startY, endX, endY);

    // Each index in prev is a node that holds its previous node prev[D] = A if the shortest path so far to D came from A.
    vector<GraphNodeIndex> prev(graph.getNodeCount(), -1);
    vector<GraphEdgeIndex> pathEdges(graph.getEdgeCount(), -1);

    // pq.first = distance to start node, pq.second = the nodeID; 
    priority_queue<pair<int, GraphNodeIndex>, vector<pair<int, GraphNodeIndex>>, greater<pair<int, GraphNodeIndex>>> minPQ;
    minPQ.push(make_pair(0, startNodeIndex));


    while (!minPQ.empty()) {
        std::pair<int, GraphNodeIndex> v = minPQ.top();
        minPQ.pop();

        GraphNode currentNode = graph.getNode(v.second);

        for (auto edgeIndex : currentNode.outEdges) {
            GraphEdge edge = graph.getEdge(edgeIndex);
            GraphNodeIndex targetNodeIndex = edge.to;
            GraphNode nextNode = graph.getNode(targetNodeIndex);
            double nextNodeX = nextNode.data.offsetLon;
            double nextNodeY = nextNode.data.offsetLat;

            // Calculate the distance from the start node to the current node.
            // And update if the new distance is shorter.
            long long int distanceFromStart = edge.weight + weights[v.second];
            if (distanceFromStart < weights[targetNodeIndex]) {
                weights[targetNodeIndex] = distanceFromStart;
                // Calculate the A* cost and update if the new cost is shorter.
                double heuristicDistance = distanceBetweenPoints(nextNodeX, nextNodeY, endX, endY);
                double heuristicMeters = degreesToMeters(heuristicDistance);
                long long int aStarCost = distanceFromStart + heuristicMeters;
                heuristic[targetNodeIndex] = aStarCost;
                prev[targetNodeIndex] = v.second;
                pathEdges[targetNodeIndex] = edgeIndex;  // Store the edge index so we can reconstruct the path later.
                minPQ.push(make_pair(aStarCost, targetNodeIndex));
            }
        }

        // Stop early if we have reached the end node to avoid unnecessary computation.
        if (v.second == endNodeIndex) {
            vector<GraphEdgeIndex> path;
            vector<GraphNodeIndex> pathNodes;
            GraphNodeIndex current = endNodeIndex;
            while (current != -1) {
                pathNodes.push_back(current);
                path.push_back(pathEdges[current]);
                current = prev[current];
            }

            reverse(pathNodes.begin(), pathNodes.end());
            reverse(path.begin(), path.end());
            path.erase(path.begin());   // Since the start node has no edge leading to it, we remove the first edge which has -1.

            // TESTING: This loop prints the path of edges, and the nodes in the path.
            for (auto e : path) {
                auto edge = graph.getEdge(e);
                std::cout << "EdgeID: " << e << std::endl;
            }
            MapGeometry mapGeometry;
            for (auto e : pathNodes) {
                auto node = graph.getNode(e);
                auto globalLonLat = mapGeometry.unoffsetGeoVector({node.data.offsetLon, node.data.offsetLat});
                std::cout << "Node: " << e << " at " << globalLonLat.y << " " << globalLonLat.x << std::endl;
            }

            return path;
        }
    }

    return vector<GraphEdgeIndex>();    // Empty vector if no path exists.
}

/**
 * Finds a shortest path between origin and destination using the selected algorithm
 * 
 * @return The shortest path
*/
vector<GraphEdgeIndex> findShortestPath(sf::Vector2<double> offsetLonLatOrigin, sf::Vector2<double> offsetLonLatDestination, AlgoName algorithm, MapGraph& mapGraph, MapGeometry& mapGeometry) {
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
        return aStarSearch(startNodeIndex, endNodeIndex, mapGraph);
    }
}
