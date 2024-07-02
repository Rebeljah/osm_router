#pragma once

#include <sqlite_orm/sqlite_orm.h>

namespace sql
{

    struct Chunk
    {
        std::string id;
        int row;
        int col;
        float offsetLatTop;
        float offsetLonLeft;
        int numNodes;
        int numEdges;
    };

    struct Edge
    {
        int id;
        int osmId;
        std::string chunkId;
        int sourceNodeId;
        int targetNodeId;
        double pathLengthMeters;
        int pathFoot;
        int pathCarFwd;
        int pathCarBwd;
        int pathBikeFwd;
        int pathBikeBwd;
        int pathTrain;
        std::string pathOffsetPoints;
    };

    struct Node
    {
        int id;
        std::string chunkId;
        double offsetLon;
        double offsetLat;
        int numOutEdges;
        int numInEdges;
    };

    inline auto loadStorage(std::string dbPath)
    {
        using namespace sqlite_orm;
        #define mt make_table
        #define mc make_column
        #define fk foreign_key
        #define mi make_index

        return make_storage(
            dbPath,
            mi("idx_node_offset_lon", &Node::offsetLon),
            mi("idx_node_offset_lat", &Node::offsetLat),
            mi("idx_node_chunk_id", &Node::chunkId),

            mi("idx_edge_source_node_id", &Edge::sourceNodeId),
            mi("idx_edge_target_node_id", &Edge::targetNodeId),
            mi("idx_edge_chunk_id", &Edge::chunkId),

            mi("idx_chunk_row", &Chunk::row),
            mi("idx_chunk_col", &Chunk::col),
            mi("idx_chunk_offset_lat_top", &Chunk::offsetLatTop),
            mi("idx_chunk_offset_lon_left", &Chunk::offsetLonLeft),

            mt("chunk",
               mc("id", &Chunk::id, primary_key()),
               mc("row", &Chunk::row),
               mc("col", &Chunk::col),
               mc("offset_lat_top", &Chunk::offsetLatTop),
               mc("offset_lon_left", &Chunk::offsetLonLeft),
               mc("num_nodes", &Chunk::numNodes),
               mc("num_edges", &Chunk::numEdges)),               

            mt("edge",
               mc("id", &Edge::id, primary_key().autoincrement()),
               mc("osm_id", &Edge::osmId),
               mc("chunk_id", &Edge::chunkId),
               mc("source_node_id", &Edge::sourceNodeId),
               mc("target_node_id", &Edge::targetNodeId),
               mc("path_length_meters", &Edge::pathLengthMeters),
               mc("path_foot", &Edge::pathFoot),
               mc("path_car_fwd", &Edge::pathCarFwd),
               mc("path_car_bwd", &Edge::pathCarBwd),
               mc("path_bike_fwd", &Edge::pathBikeFwd),
               mc("path_bike_bwd", &Edge::pathBikeBwd),
               mc("path_train", &Edge::pathTrain),
               mc("path_offset_points", &Edge::pathOffsetPoints),
               fk(&Edge::sourceNodeId).references(&Node::id),
               fk(&Edge::targetNodeId).references(&Node::id),
               fk(&Edge::chunkId).references(&Chunk::id)),

            mt("node",
               mc("id", &Node::id, primary_key()),
               mc("chunk_id", &Node::chunkId),
               mc("offset_lon", &Node::offsetLon),
               mc("offset_lat", &Node::offsetLat),
               mc("num_out_edges", &Node::numOutEdges),
               mc("num_in_edges", &Node::numInEdges),
               fk(&Node::chunkId).references(&Chunk::id)));
    }

    // We need to use decltype here because the type returned by sqlite_orm::make_storage
    // depends on the indices and tables that it contains. Decltype will make a new
    // type that matches what is actually returned by loadStorage so that the type
    // can be used as a member variable of other classes.
    using Storage = decltype(loadStorage(""));
};
