from dataclasses import dataclass
import decimal
import tomllib
from typing import Iterable
import sqlite3
from collections import defaultdict

from tqdm import tqdm

decimal.getcontext().traps[decimal.FloatOperation] = True
decimal.getcontext().prec = 25

type ID = int


@dataclass
class Point:
    x: decimal.Decimal
    y: decimal.Decimal


@dataclass
class Edge:
    # SQL COLUMNS
    osm_id: ID
    chunk_id: str
    source_node_id: ID
    target_node_id: ID
    path_length_meters: decimal.Decimal
    path_foot: int
    path_car_fwd: int
    path_car_bwd: int
    path_bike_fwd: int
    path_bike_bwd: int
    path_train: int
    path_offset_points: list[Point]

    def sql_tuple(self) -> tuple:
        return (
            None,
            self.osm_id,
            self.chunk_id,
            self.source_node_id,
            self.target_node_id,
            str(self.path_length_meters),
            self.path_foot,
            self.path_car_fwd,
            self.path_car_bwd,
            self.path_bike_fwd,
            self.path_bike_bwd,
            self.path_train,
            ','.join(str(point.x) + ' ' + str(point.y) for point in self.path_offset_points)
        )


@dataclass
class Node:
    # SQL COLUMNS
    id: ID
    chunk_id: str
    offset_lon: decimal.Decimal
    offset_lat: decimal.Decimal
    num_out_edges: int
    num_in_edges: int

    def sql_tuple(self) -> tuple:
        return (
            self.id,
            self.chunk_id,
            str(self.offset_lon),
            str(self.offset_lat),
            self.num_out_edges,
            self.num_in_edges
        )


@dataclass
class Chunk:
    # SQL COLUMNS
    id: str
    row: int
    col: int
    offset_lat_top: decimal.Decimal
    offset_lon_left: decimal.Decimal
    num_nodes: int
    num_edges: int

    def sql_tuple(self) -> tuple:
        return (
            self.id,
            self.row,
            self.col,
            str(self.offset_lat_top),
            str(self.offset_lon_left),
            self.num_nodes,
            self.num_edges
        )


@dataclass
class Rect:
    top: decimal.Decimal
    left: decimal.Decimal
    width: decimal.Decimal
    height: decimal.Decimal


def chunk_id(row, col) -> str:
    return f"{row},{col}"


def chunk_grid_pos(point: Point, chunk_size: decimal.Decimal) -> tuple[int, int]:
    # returns (row, col)
    return (
        int((point.y / chunk_size).to_integral_value(decimal.ROUND_FLOOR)),
        int((point.x / chunk_size).to_integral_value(decimal.ROUND_FLOOR))
    )


path_descriptor_to_int = {"Forbidden": 0, "Allowed": 1, "Residential": 2, "Tertiary": 3, "Secondary": 4, "Primary": 5, "Trunk": 6, "Motorway": 7, "Track": 8, "Lane": 8}


def parse_wkt_linestring(wkt: str) -> list[Point]:
    #  from '"LINESTRING(-81.3862789 28.6238899, -81.3863183 28.6244381)"'
    points = wkt.strip()[12:-2].split(', ')
    return [Point(*map(decimal.Decimal, point.split())) for point in points]


def offset_edge_path(path: list[Point], map_bbox: Rect) -> None:
    for point in path:
        point.x -= map_bbox.left
        point.y = map_bbox.top - point.y


def chunk_generator(chunk_size: decimal.Decimal, map_bbox: Rect, chunk_num_nodes, chunk_num_edges):
    n_rows = int((map_bbox.height / chunk_size).to_integral_value(decimal.ROUND_UP))
    n_cols = int((map_bbox.width / chunk_size).to_integral_value(decimal.ROUND_UP))
    for row in tqdm(range(n_rows), "making and inserting chunk grid rows"):
        for col in range(n_cols):
            yield Chunk(chunk_id(row, col), row, col, row * chunk_size, col * chunk_size, chunk_num_nodes[chunk_id(row, col)], chunk_num_edges[chunk_id(row, col)])


def node_generator(map_bbox: Rect, chunk_size: decimal.Decimal, node_num_out_edges, node_num_in_edges, chunk_num_nodes):
    with open("./data/nodes_bboxed.csv") as f:
        n_nodes = sum(1 for _ in f) - 1

    with open("./data/nodes_bboxed.csv") as f:
        f.readline()  # skip columns headers
        for line in tqdm(f, 'making and inserting nodes', n_nodes):
            cols = line.split(',')
            node = Node(
                int(cols[0]),
                '',
                decimal.Decimal(cols[1]),
                decimal.Decimal(cols[2]),
                0,
                0
            )

            # offset node lat/lon coordinates
            node.offset_lat = map_bbox.top - node.offset_lat
            node.offset_lon = node.offset_lon - map_bbox.left

            # set chunk id and increment chunk num_nodes
            row, col = chunk_grid_pos(Point(node.offset_lon, node.offset_lat), chunk_size)

            node.chunk_id = chunk_id(row, col)
            node.num_out_edges = node_num_out_edges[node.id]
            node.num_in_edges = node_num_in_edges[node.id]

            #Skip nodes that are not connected to at least one car-accessible edge
            if node.num_out_edges == 0 and node.num_in_edges == 0:
                continue

            chunk_num_nodes[chunk_id(row, col)] += 1

            yield node


def edge_generator(chunk_size: decimal.Decimal, map_bbox: Rect, node_num_out_edges, node_num_in_edges, chunk_num_edges):
    with open("./data/edges_bboxed.csv") as f:
        n_edges = sum(1 for _ in f) - 1

    with open("./data/edges_bboxed.csv") as f:
        f.readline()  # skip columns headers
        for line in tqdm(f, 'making and inserting edges', n_edges):
            line = line.strip()
            wkt_start = line.find(',"L')
            cols = line[:wkt_start].split(',')[1:]  # skip id column (1st col)
            path = parse_wkt_linestring(line[wkt_start+1:])
            offset_edge_path(path, map_bbox)

            #Skip edges with car_forward and car_backward set to "Forbidden"
            if path_descriptor_to_int[cols[5]] == 0 and path_descriptor_to_int[cols[6]] == 0:
                continue

            # edge belongs to chunk where path starts
            chunk_row, chunk_col = chunk_grid_pos(path[0], chunk_size)

            edge = Edge(
                int(cols[0]),
                chunk_id(chunk_row, chunk_col),
                int(cols[1]),
                int(cols[2]),
                decimal.Decimal(cols[3]),
                path_descriptor_to_int[cols[4]],
                path_descriptor_to_int[cols[5]],
                path_descriptor_to_int[cols[6]],
                path_descriptor_to_int[cols[7]],
                path_descriptor_to_int[cols[8]],
                path_descriptor_to_int[cols[9]],
                path,
            )

            # update node number of edges in/out
            node_num_in_edges[edge.target_node_id] += 1
            node_num_out_edges[edge.source_node_id] += 1

            # update chunk num_edges
            chunk_num_edges[chunk_id(chunk_row, chunk_col)] += 1

            yield edge

def insert_rows(tbl_name: str, rows: Iterable[Chunk | Node | Edge], con: sqlite3.Connection) -> None:
    cur = con.cursor()

    for row in rows:
        values = row.sql_tuple()
        stmnt = f'INSERT INTO {tbl_name} VALUES({",".join(["?"]*len(values))})'
        cur.execute(stmnt, values)

    con.commit()


def main():
    with open('./config/config.toml', 'rb') as f:
        config = tomllib.load(f, parse_float=decimal.Decimal)
    
    chunk_size = config['map']['chunk_size']

    map_bbox = Rect(
        config['map']['bbox_top'],
        config['map']['bbox_left'],
        config['map']['bbox_right'] - config['map']['bbox_left'],
        config['map']['bbox_top'] - config['map']['bbox_bottom']
    )
    
    chunk_num_edges = defaultdict(int)
    chunk_num_nodes = defaultdict(int)
    node_num_in_edges = defaultdict(int)
    node_num_out_edges = defaultdict(int)

    with sqlite3.connect("./db/map.db") as con:
        insert_rows('edge', edge_generator(chunk_size, map_bbox, node_num_out_edges, node_num_in_edges, chunk_num_edges), con)
        insert_rows('node', node_generator(map_bbox, chunk_size, node_num_out_edges, node_num_in_edges, chunk_num_nodes), con)
        insert_rows('chunk', chunk_generator(chunk_size, map_bbox, chunk_num_nodes, chunk_num_edges), con)

main()