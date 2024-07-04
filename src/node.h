#pragma once

#include <vector>
#include <cmath>

#include "sql.h"
#include "edge.h"

using std::vector;

struct Node
{
    sql::Node data;
    vector<Edge> edgesOut;

    Node(sql::Node data) : data(data) {}
};

/*
Returns the node pointer which is closest to the given longitude / latitude point.
@param nodes: Vector of Node* to consider
@param lonLatPoint: geographic point
@returns Node* with the closest coordinate to the given coordinate.
*/
Node *findNearestNode(vector<Node *> nodes, double longitude, double latitude)
{
    using std::sqrt;
    using std::pow;

    if (nodes.empty())
        throw std::domain_error("Vector of nodes is empty");
    
    double x0 = longitude;
    double y0 = latitude;

    double smallestDistance = 100000000;  // no distance will be larger than this
    Node *pClosestNode = nullptr;

    for (auto pNode : nodes)
    {
        double x1 = pNode->data.offsetLon;
        double y1 = pNode->data.offsetLat;

        // euclidean distance with pythagorean theorem
        double distance = sqrt(pow(x1 - x0, 2) + pow(y1 - y0, 2));
        
        if (distance < smallestDistance)
        {
            smallestDistance = distance;
            pClosestNode = pNode;
        }

    }

    return pClosestNode;
}