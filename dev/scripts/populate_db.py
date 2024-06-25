import sqlite3
import re
import tomllib
import math
from collections import namedtuple

NODE_CSV = "./data/nodes_bboxed.csv"
EDGE_CSV = "./data/edges_bboxed.csv"
DB_NAME = "./db/map.db"
CONFIG_FILE = "./config/config.toml"

con = sqlite3.connect(DB_NAME, autocommit=False)
cur = con.cursor()

#load config data
with open(CONFIG_FILE, "rb") as f:
    config = tomllib.load(f)


map_width = config["map"]["bbox_right"] - config["map"]["bbox_left"]
map_height = config["map"]["bbox_top"] - config["map"]["bbox_bottom"]
chunk_size = config["map"]["chunk_size"]
map_origin_lat = config["map"]["bbox_top"]
map_origin_lon = config["map"]["bbox_left"]

class Node:
    def __init__(self, id, longitude, latitude, chunk, n_edges_out, n_edges_in):
        self.id = id
        self.longitude = longitude
        self.latitude = latitude
        self.chunk = chunk
        self.n_edges_out = n_edges_out
        self.n_edges_in = n_edges_in

class Chunk:
    def __init__(self, id, grid_row, grid_col, left, right, bottom, top, n_nodes, n_edges):
        self.id = id
        self.grid_row = grid_row
        self.grid_col = grid_col
        self.left = left
        self.right = right
        self.bottom = bottom
        self.top = top
        self.n_nodes = n_nodes
        self.n_edges = n_edges

class Edge:
    def __init__(self, id, osm_id, source_node, target_node, length, foot, car_forward, car_backward, bike_forward, bike_backward, train, wkt_linestring, chunk):
        self.id = id
        self.osm_id = osm_id
        self.source_node = source_node
        self.target_node = target_node
        self.length = length
        self.foot = foot
        self.car_forward = car_forward
        self.car_backward = car_backward
        self.bike_forward = bike_forward
        self.bike_backward = bike_backward
        self.train = train
        self.wkt_linestring = wkt_linestring
        self.chunk = chunk


chunks: dict[tuple[int, int], Chunk] = {}
chunks_id_to_row_col: dict[int, tuple[int, int]] = {}
nodes: dict[int, Node] = {}
edges: dict[str, Edge] = {}

#create chunks
chunk_id = 0
last_chunk_col = math.ceil(map_width / chunk_size)
last_chunk_row = math.ceil(map_height / chunk_size)
for col in range(0, last_chunk_col + 1):
    for row in range(0, last_chunk_row + 1):
        # build chunk bbox
        left = map_origin_lon + col * chunk_size
        right = left + chunk_size
        top = map_origin_lat - row * chunk_size
        bottom = top - chunk_size
        chunks[(row, col)] = Chunk(chunk_id, row, col, left, right, bottom, top, 0, 0)
        chunks_id_to_row_col[chunk_id] = (row, col)
        chunk_id += 1

#create nodes
with open(NODE_CSV) as f:
    f.readline()  # Skip header
    for line in f:
        parts = line.split(",")
        node_id = int(parts[0])
        lon = float(parts[1])
        lat = float(parts[2])
        chunk_row = math.floor((map_origin_lat - lat) / chunk_size)
        chunk_col = math.floor((lon - map_origin_lon) / chunk_size)
        chunk_id = chunks[chunk_row, chunk_col].id
        nodes[node_id] = Node(node_id, lon, lat, chunk_id, 0, 0)

#create edges
with open(EDGE_CSV) as f:
    f.readline()  # Skip header
    for line in f:
        m = re.match(r"(.+),\"LINESTRING\((.+)\)\"", line)
        data = m.group(1).split(",") + [m.group(2)] + [0]
        edge = Edge(*data)
        if (int(edge.source_node) in nodes and int(edge.target_node) in nodes):
            edges[edge.id] = edge

#update chunks with number of nodes
for node in nodes.values():
    row, col = chunks_id_to_row_col[node.chunk]
    chunks[row, col].n_nodes += 1

#update chunks with number of edges
for edge in edges.values():
    src_node = nodes[int(edge.source_node)]
    row, col = chunks_id_to_row_col[src_node.chunk]
    chunks[row, col].n_edges += 1

#update nodes with edges in/out
for edge in edges.values():
    src_node = nodes[int(edge.source_node)]
    tgt_node = nodes[int(edge.target_node)]
    src_node.n_edges_out += 1
    tgt_node.n_edges_in += 1

#update edges with chunk id
for edge in edges.values():
    src_node = nodes[int(edge.source_node)]
    edge.chunk = src_node.chunk


#insert chunks
for chunk in chunks.values():
    cur.execute("INSERT INTO chunk values (?,?,?,?,?,?,?,?,?)", (
        chunk.id,
        chunk.grid_row,
        chunk.grid_col,
        chunk.left,
        chunk.right,
        chunk.bottom,
        chunk.top,
        chunk.n_nodes,
        chunk.n_edges
    ))
con.commit()

#insert nodes
for node in nodes.values():
    cur.execute("INSERT INTO node values (?,?,?,?,?,?)", (
        node.id,
        node.longitude,
        node.latitude,
        node.chunk,
        node.n_edges_out,
        node.n_edges_in
    ))
con.commit()

#insert edges
for edge in edges.values():
    cur.execute("INSERT INTO edge values (?,?,?,?,?,?,?,?,?,?,?,?,?)", (
        edge.id,
        edge.osm_id,
        edge.source_node,
        edge.target_node,
        edge.length,
        edge.foot,
        edge.car_forward,
        edge.car_backward,
        edge.bike_forward,
        edge.bike_backward,
        edge.train,
        edge.wkt_linestring,
        edge.chunk
    ))
con.commit()

con.close()
