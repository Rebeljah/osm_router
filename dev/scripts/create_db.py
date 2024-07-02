import sqlite3

DB_NAME = "./db/map.db"

con = sqlite3.connect(DB_NAME, autocommit=False)
cur = con.cursor()

sql_script = """
    CREATE TABLE IF NOT EXISTS node (
        id INTEGER PRIMARY KEY,
        chunk_id STRING,
        offset_lon REAL,
        offset_lat REAL,
        num_out_edges INTEGER,
        num_in_edges INTEGER,
        FOREIGN KEY(chunk_id) REFERENCES chunk(id)
    );
    CREATE INDEX IF NOT EXISTS idx_node_offset_lon ON node (offset_lon);
    CREATE INDEX IF NOT EXISTS idx_node_offset_lat ON node (offset_lat);
    CREATE INDEX IF NOT EXISTS idx_node_chunk_id ON node (chunk_id);

    CREATE TABLE IF NOT EXISTS edge (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        osm_id INTEGER,
        chunk_id STRING,
        source_node_id INTEGER,
        target_node_id INTEGER,
        path_length_meters REAL,
        path_foot INTEGER,
        path_car_fwd INTEGER,
        path_car_bwd INTEGER,
        path_bike_fwd INTEGER,
        path_bike_bwd INTEGER,
        path_train INTEGER,
        path_offset_points TEXT,
        FOREIGN KEY(source_node_id) REFERENCES node(id),
        FOREIGN KEY(target_node_id) REFERENCES node(id),
        FOREIGN KEY(chunk_id) REFERENCES chunk(id)
    );
    CREATE INDEX IF NOT EXISTS idx_edge_source_node_id ON edge (source_node_id);
    CREATE INDEX IF NOT EXISTS idx_edge_target_node_id ON edge (target_node_id);
    CREATE INDEX IF NOT EXISTS idx_edge_chunk_id ON edge (chunk_id);

    CREATE TABLE IF NOT EXISTS chunk (
        id STRING PRIMARY KEY,
        row INTEGER,
        col INTEGER,
        offset_lat_top REAL,
        offset_lon_left REAL,
        num_nodes INTEGER,
        num_edges INTEGER
    );
    CREATE INDEX IF NOT EXISTS idx_chunk_row ON chunk (row);
    CREATE INDEX IF NOT EXISTS idx_chunk_col ON chunk (col);
    CREATE INDEX IF NOT EXISTS idx_chunk_offset_lat_top ON chunk (offset_lat_top);
    CREATE INDEX IF NOT EXISTS idx_chunk_offset_lon_left ON chunk (offset_lon_left);
"""
cur.executescript(sql_script)

con.commit()
con.close()
