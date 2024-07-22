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