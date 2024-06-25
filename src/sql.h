#include <sqlite_orm/sqlite_orm.h>

namespace sql
{

    struct Chunk
    {
        int id;
        int gridRow;
        int gridCol;
        double left;
        double right;
        double bottom;
        double top;
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
        std::string foot;
        std::string carForward;
        std::string carBackward;
        std::string bikeForward;
        std::string bikeBackward;
        std::string train;
        std::string wktLinestring;
        int chunk;
    };

    struct Node
    {
        int id;
        double longitude;
        double latitude;
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
            mi("idx_node_latitude", &Node::latitude),
            mi("idx_node_longitude", &Node::longitude),
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
            mi("idx_chunk_left", &Chunk::left),
            mi("idx_chunk_right", &Chunk::right),
            mi("idx_chunk_bottom", &Chunk::bottom),
            mi("idx_chunk_top", &Chunk::top),

            mt("chunk",
               mc("id", &Chunk::id, primary_key().autoincrement()),
               mc("grid_row", &Chunk::gridRow),
               mc("grid_col", &Chunk::gridCol),
               mc("left", &Chunk::left),
               mc("right", &Chunk::right),
               mc("bottom", &Chunk::bottom),
               mc("top", &Chunk::top),
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
               mc("wkt_linestring", &Edge::wktLinestring),
               mc("chunk", &Edge::chunk),
               fk(&Edge::sourceNode).references(&Node::id),
               fk(&Edge::targetNode).references(&Node::id),
               fk(&Edge::chunk).references(&Chunk::id)),

            mt("node",
               mc("id", &Node::id, primary_key()),
               mc("longitude", &Node::longitude),
               mc("latitude", &Node::latitude),
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
