#include <sqlite_orm/sqlite_orm.h>

namespace sql
{

    struct Chunk
    {
        int id;
        int gridRow;
        int gridCol;
        double leftOffset;
        double topOffset;
        double size;
        int nNodes;
        int nEdges;
    };

    struct Edge
    {
        std::string id;
        int osmId;
        int sourceNode;
        int targetNode;
        double length;
        int foot;
        int carForward;
        int carBackward;
        int bikeForward;
        int bikeBackward;
        int train;
        std::string wktLinestringOffset;
        int chunk;
    };

    struct Node
    {
        int id;
        double offsetLongitude;
        double offsetLatitude;
        int chunk;
        int nEdgesOut;
        int nEdgesIn;
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
            mi("idx_node_offset_longitude", &Node::offsetLongitude),
            mi("idx_node_offset_latitude", &Node::offsetLatitude),
            mi("idx_node_chunk", &Node::chunk),

            mi("idx_edge_source_node", &Edge::sourceNode),
            mi("idx_edge_target_node", &Edge::targetNode),
            mi("idx_edge_foot", &Edge::foot),
            mi("idx_edge_car_forward", &Edge::carForward),
            mi("idx_edge_car_backward", &Edge::carBackward),
            mi("idx_edge_bike_forward", &Edge::bikeForward),
            mi("idx_edge_bike_backward", &Edge::bikeBackward),
            mi("idx_edge_chunk", &Edge::chunk),

            mi("idx_chunk_grid_row", &Chunk::gridRow),
            mi("idx_chunk_grid_col", &Chunk::gridCol),
            mi("idx_chunk_left_offset", &Chunk::leftOffset),
            mi("idx_chunk_top_offset", &Chunk::topOffset),

            mt("chunk",
               mc("id", &Chunk::id, primary_key().autoincrement()),
               mc("grid_row", &Chunk::gridRow),
               mc("grid_col", &Chunk::gridCol),
               mc("top_offset", &Chunk::topOffset),
               mc("left_offset", &Chunk::leftOffset),
               mc("size", &Chunk::size),
               mc("n_nodes", &Chunk::nNodes),
               mc("n_edges", &Chunk::nEdges)),               

            mt("edge",
               mc("id", &Edge::id, primary_key()),
               mc("osm_id", &Edge::osmId),
               mc("source_node", &Edge::sourceNode),
               mc("target_node", &Edge::targetNode),
               mc("length", &Edge::length),
               mc("foot", &Edge::foot),
               mc("car_forward", &Edge::carForward),
               mc("car_backward", &Edge::carBackward),
               mc("bike_forward", &Edge::bikeForward),
               mc("bike_backward", &Edge::bikeBackward),
               mc("train", &Edge::train),
               mc("wkt_linestring_offset", &Edge::wktLinestringOffset),
               mc("chunk", &Edge::chunk),
               fk(&Edge::sourceNode).references(&Node::id),
               fk(&Edge::targetNode).references(&Node::id),
               fk(&Edge::chunk).references(&Chunk::id)),

            mt("node",
               mc("id", &Node::id, primary_key()),
               mc("offset_longitude", &Node::offsetLongitude),
               mc("offset_latitude", &Node::offsetLatitude),
               mc("chunk", &Node::chunk),
               mc("n_edges_out", &Node::nEdgesOut),
               mc("n_edges_in", &Node::nEdgesIn),
               fk(&Node::chunk).references(&Chunk::id)));
    }

    // We need to use decltype here because the type returned by sqlite_orm::make_storage
    // depends on the indices and tables that it contains. Decltype will make a new
    // type that matches what is actually returned by loadStorage so that the type
    // can be used as a member variable of other classes.
    using Storage = decltype(loadStorage(""));
};
