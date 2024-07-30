import sqlite3
from collections import defaultdict

def delete_forbidden_edges(con: sqlite3.Connection):
    cur = con.cursor()
    cur.execute("DELETE FROM edge WHERE path_car_fwd = 0 AND path_car_bwd = 0")
    con.commit()

def delete_unconnected_nodes(con: sqlite3.Connection):
    cur = con.cursor()
    cur.execute("""
        DELETE FROM node
        WHERE id NOT IN (
            SELECT DISTINCT source_node_id FROM edge
            UNION
            SELECT DISTINCT target_node_id FROM edge
        )
    """)
    con.commit()

def main():
    with sqlite3.connect("./db/map.db") as con:
        delete_forbidden_edges(con)
        delete_unconnected_nodes(con)

if __name__ == "__main__":
    main()