#pragma once

#include <vector>

#include "sql.h"
#include "edge.h"

using std::vector;

typedef int NodeID;

struct Node
{
    sql::Node data;
    vector<Edge> edgesOut;

    Node(sql::Node data) : data(data) {}
};