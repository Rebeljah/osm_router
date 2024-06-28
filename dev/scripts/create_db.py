import sqlite3

DB_NAME = "./db/map.db"

con = sqlite3.connect(DB_NAME, autocommit=False)
cur = con.cursor()

"""
This database is built for holding data from the osm4routing rust package.

NODE:
 id: unique id
 longitude: in decimal degrees
 latitude: in decimal degrees

EDGE:
 id: unique id created by osm4routing
 osm_id: osm id can be used to lookup additional data on edge using osm API(?)
 source_node/target_node: id of start and end nodes (foreign keys).
 length: length in meters of the edge
 foot: one of 'Forbidden', 'Allowed' mapped to 0 - 1
 car_forward/backward: one of 'Forbidden', 'Residential', 'Tertiary', 'Secondary', 'Primary', 'Trunk', 'Motorway' mapped to 0, 2 - 7
 bike_forward/backward: one of 'Forbidden', 'Allowed', 'Track', 'Lane' mapped to 0, 8 - 9
 train: one of 'Forbidden', 'Allowed' mapped to 0 - 1
 wkt_linestring: lat/lon decimal degree point chain representing the path of the edge from source node to target node e.g: '-81.3862789 28.6238899, -81.3863183 28.6244381, ...'

CHUNK:
 grid_row/col: 0-based grid location of chunk. (top-left corner of map is 0,0)
 left/right/bottom/top: lat or lon degrees defining the chunk bounding box
"""

sql_script = """
    -- Create node table with primary key and indexes
    CREATE TABLE IF NOT EXISTS node (
        id INTEGER PRIMARY KEY,
        offset_longitude REAL,
        offset_latitude REAL,
        chunk INTEGER,
        n_edges_out INTEGER,
        n_edges_in INTEGER,
        FOREIGN KEY(chunk) REFERENCES chunk(id)
    );
    CREATE INDEX IF NOT EXISTS idx_node_offset_longitude ON node (offset_longitude);
    CREATE INDEX IF NOT EXISTS idx_node_offset_latitude ON node (offset_latitude);
    CREATE INDEX IF NOT EXISTS idx_node_chunk ON node (chunk);

    -- Create edge table with foreign keys and indexes
    CREATE TABLE IF NOT EXISTS edge (
        id TEXT PRIMARY KEY,
        osm_id INTEGER,
        source_node INTEGER,
        target_node INTEGER,
        length REAL,
        foot INTEGER,
        car_forward INTEGER,
        car_backward INTEGER,
        bike_forward INTEGER,
        bike_backward INTEGER,
        train INTEGER,
        wkt_linestring_offset TEXT,
        chunk INTEGER,
        FOREIGN KEY(source_node) REFERENCES node(id),
        FOREIGN KEY(target_node) REFERENCES node(id),
        FOREIGN KEY(chunk) REFERENCES chunk(id)
    );
    CREATE INDEX IF NOT EXISTS idx_edge_source_node ON edge (source_node);
    CREATE INDEX IF NOT EXISTS idx_edge_target_node ON edge (target_node);
    CREATE INDEX IF NOT EXISTS idx_edge_foot ON edge (foot);
    CREATE INDEX IF NOT EXISTS idx_edge_car_forward ON edge (car_forward);
    CREATE INDEX IF NOT EXISTS idx_edge_car_backward ON edge (car_backward);
    CREATE INDEX IF NOT EXISTS idx_edge_bike_forward ON edge (bike_forward);
    CREATE INDEX IF NOT EXISTS idx_edge_bike_backward ON edge (bike_backward);
    CREATE INDEX IF NOT EXISTS idx_edge_chunk ON edge (chunk);

    -- Create chunk table with lat/lon bbox values
    CREATE TABLE IF NOT EXISTS chunk (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        grid_row INTEGER,
        grid_col INTEGER,
        left_offset REAL,
        top_offset REAL,
        size REAL,
        n_nodes INTEGER,
        n_edges INTEGER
    );
    CREATE INDEX IF NOT EXISTS idx_chunk_grid_row ON chunk (grid_row);
    CREATE INDEX IF NOT EXISTS idx_chunk_grid_col ON chunk (grid_col);
    CREATE INDEX IF NOT EXISTS idx_chunk_left_offset ON chunk (left_offset);
    CREATE INDEX IF NOT EXISTS idx_chunk_top_offset ON chunk (top_offset);
"""
cur.executescript(sql_script)

con.commit()
con.close()
