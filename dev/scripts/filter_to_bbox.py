import tomllib

CONFIG_FILE = "./config/config.toml"
NODE_CSV_SRC = "./data/nodes.csv"
NODE_CSV_TARGET = "./data/nodes_bboxed.csv"
EDGE_CSV_SRC = "./data/edges.csv"
EDGE_CSV_TARGET = "./data/edges_bboxed.csv"

#load config data
with open(CONFIG_FILE, "rb") as f:
    config = tomllib.load(f)["map"]

filtered_node_ids = set()

# filter nodes to those in the bbox
with open(NODE_CSV_SRC) as inf:
    with open(NODE_CSV_TARGET, "w") as outf:
        outf.write(inf.readline()) # header
        for line in inf:
            id_, lon, lat = line.split(",")
            if (config['bbox_left'] <= float(lon) <= config['bbox_right'] and config['bbox_bottom'] <= float(lat) <= config['bbox_top']):
                outf.write(line)
                filtered_node_ids.add(id_)

# filter edges to those whose source and target are in the bbox
with open(EDGE_CSV_SRC) as inf:
    with open(EDGE_CSV_TARGET, "w") as outf:
        outf.write(inf.readline()) #header
        for line in inf:
            source_node_id, target_node_id = line.split(",")[2:4]
            if (source_node_id in filtered_node_ids and target_node_id in filtered_node_ids):
                outf.write(line)
