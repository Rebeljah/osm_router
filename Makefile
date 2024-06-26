osm4routing: # converts pbf to nodes.csv and edges.csv
	cd ./data && osm4routing us-south-latest.osm.pbf

filter_csv: # filter nodes/edges to the lat/lon bbox defined in the py file
	python ./dev/scripts/filter_to_bbox.py

create_db:
	python ./dev/scripts/create_db.py

fill_db:  # fill the database with node/edge data from the CSV's defined in the populate_db.py file
	python ./dev/scripts/populate_db.py