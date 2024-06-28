import sqlite3
import re
import tomllib
import math
from tqdm import tqdm
import copy

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


def offset_wkt_linestring_from_origin(points_str):
    points = points_str.strip().split(', ')
    
    offset_points = []
    
    for point in points:
        lon, lat = map(float, point.split())
        
        # Calculate offsets
        offset_lon = lon - map_origin_lon
        offset_lat = map_origin_lat - lat
        
        offset_lon_str = format(offset_lon, '.15f')
        offset_lat_str = format(offset_lat, '.15f')
        
        offset_point_str = f'{offset_lon_str} {offset_lat_str}'
        offset_points.append(offset_point_str)

    return ', '.join(offset_points)


class Node:
    def __init__(self, id, offset_longitude, offset_latitude, chunk):
        self.id = id
        self.offset_longitude = offset_longitude
        self.offset_latitude = offset_latitude
        self.chunk = chunk
        self.edges_out = []
        self.edges_in = []

class Chunk:
    def __init__(self, id, grid_row, grid_col, left_offset, top_offset, size, n_nodes, n_edges):
        self.id = id
        self.grid_row = grid_row
        self.grid_col = grid_col
        self.left_offset = left_offset
        self.top_offset = top_offset
        self.size = size
        self.n_nodes = n_nodes
        self.n_edges = n_edges

class Edge:
    def __init__(self, id, osm_id, source_node, target_node, length, foot, car_forward, car_backward, bike_forward, bike_backward, train, wkt_linestring_offset, chunk):
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
        self.wkt_linestring_offset = wkt_linestring_offset
        self.chunk = chunk


chunks: dict[tuple[int, int], Chunk] = {}
chunks_id_to_row_col: dict[int, tuple[int, int]] = {}
nodes: dict[int, Node] = {}
edges: dict[str, Edge] = {}

#create chunks
chunk_id = 0
last_chunk_col = math.ceil(map_width / chunk_size)
last_chunk_row = math.ceil(map_height / chunk_size)
for col in tqdm(range(0, last_chunk_col + 1), "creating chunks"):
    for row in range(0, last_chunk_row + 1):
        # build chunk bbox
        left_offset = col * chunk_size
        top_offset= row * chunk_size
        chunks[(row, col)] = Chunk(chunk_id, row, col, left_offset, top_offset, chunk_size, 0, 0)
        chunks_id_to_row_col[chunk_id] = (row, col)
        chunk_id += 1

#count nodes
with(open(NODE_CSV)) as f:
    n_nodes = sum(1 for _ in f) - 1
#create nodes
with open(NODE_CSV) as f:
    f.readline()  # Skip header
    for line in tqdm(f, "creating nodes", n_nodes):
        parts = line.split(",")
        node_id = int(parts[0])
        lon = float(parts[1])
        lat = float(parts[2])
        # latitude and longitude is offset to map origin (topleft of area)
        offset_lat = map_origin_lat - lat
        offset_lon = lon - map_origin_lon
        chunk_row = math.floor((offset_lat) / chunk_size)
        chunk_col = math.floor((offset_lon) / chunk_size)
        chunk_id = chunks[chunk_row, chunk_col].id
        nodes[node_id] = Node(node_id, offset_lon, offset_lat, chunk_id)

#count edges
with(open(EDGE_CSV)) as f:
    n_edges = sum(1 for _ in f) - 1
#create edges
path_descriptor_to_int = {"Forbidden": 0, "Allowed": 1, "Residential": 2, "Tertiary": 3, "Secondary": 4, "Primary": 5, "Trunk": 6, "Motorway": 7, "Track": 8, "Lane": 8}
with open(EDGE_CSV) as f:
    f.readline()  # Skip header
    for line in tqdm(f, "creating edges", n_edges):
        m = re.match(r"(.+),\"LINESTRING\((.+)\)\"", line)
        data = m.group(1).split(",") + [offset_wkt_linestring_from_origin(m.group(2)), 0]
        edge = Edge(*data)
        edge.foot = path_descriptor_to_int[edge.foot]
        edge.car_forward = path_descriptor_to_int[edge.car_forward]
        edge.car_backward = path_descriptor_to_int[edge.car_backward]
        edge.bike_forward = path_descriptor_to_int[edge.bike_forward]
        edge.bike_backward = path_descriptor_to_int[edge.bike_backward]
        edge.train = path_descriptor_to_int[edge.train]
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
    src_node.edges_out.append(edge.id)
    tgt_node.edges_in.append(edge.id)

#update edges with chunk id
for edge in edges.values():
    src_node = nodes[int(edge.source_node)]
    edge.chunk = src_node.chunk

#compression steps

# remove nodes that only have 1 in edge and 1 out edge
# where both edges have the same path descriptors
deleted_nodes = []
for node in nodes.values():
    if len(node.edges_out) != 1 or len(node.edges_in) != 1:
        continue # not a removable node
    
    in_edge = edges[node.edges_in[0]]
    out_edge = edges[node.edges_out[0]]

    # check the edges are same type of path
    if (in_edge.foot != out_edge.foot or in_edge.car_forward != out_edge.car_forward or in_edge.car_backward != out_edge.car_backward or in_edge.bike_forward != out_edge.bike_forward or in_edge.bike_backward != out_edge.bike_backward or in_edge.train != out_edge.train):
        continue

    in_node = nodes[int(in_edge.source_node)]
    mid_node = node
    out_node = nodes[int(out_edge.target_node)]

    assert int(in_edge.target_node) == mid_node.id and int(out_edge.source_node) == mid_node.id

    combined_edge = copy.copy(in_edge)
    combined_edge.target_node = out_node.id
    out_node.edges_in[0] = combined_edge.id
    combined_edge.wkt_linestring_offset += ", " + out_edge.wkt_linestring_offset
    edges.pop(out_edge.id)
    edges[in_edge.id] = combined_edge

    combined_edge.length += out_edge.length

    # update chunk node count
    row, col = chunks_id_to_row_col[mid_node.chunk]
    chunks[row, col].n_nodes -= 1
    assert chunks[row, col].n_nodes >= 0

    #update chunk edge count
    row, col = chunks_id_to_row_col[int(out_edge.chunk)]
    chunks[row, col].n_edges -= 1
    assert chunks[row, col].n_edges >= 0

    deleted_nodes.append(mid_node.id)

for node_id in deleted_nodes:
    nodes.pop(node_id)

#insert chunks
for chunk in tqdm(chunks.values(), "inserting chunks"):
    cur.execute("INSERT INTO chunk values (?,?,?,?,?,?,?,?)", (
        chunk.id,
        chunk.grid_row,
        chunk.grid_col,
        chunk.left_offset,
        chunk.top_offset,
        chunk.size,
        chunk.n_nodes,
        chunk.n_edges
    ))
con.commit()

#insert nodes
for node in tqdm(nodes.values(), "inserting nodes"):
    cur.execute("INSERT INTO node values (?,?,?,?,?,?)", (
        node.id,
        node.offset_longitude,
        node.offset_latitude,
        node.chunk,
        len(node.edges_out),
        len(node.edges_in)
    ))
con.commit()

#insert edges
for edge in tqdm(edges.values(), "inserting egdes"):
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
        edge.wkt_linestring_offset,
        edge.chunk
    ))
con.commit()

con.close()
