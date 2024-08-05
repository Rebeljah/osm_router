### Proposal Gdoc:
- https://docs.google.com/document/d/1JEEAZYAosy_jTsWOz1q2IIQ6D6_zGJDuMbgMNqxPP5w/edit?usp=sharing
### Links to download CSV data for building the database:
- edges: https://drive.google.com/file/d/1b_NCY28mntp-kgoj-be7G_XBpvjarWO5/view?usp=drive_link
- nodes: https://drive.google.com/file/d/1fsxVeypoj_RyhSubA9on3KOscupBkYcy/view?usp=drive_link

# Instructions for setting up the repo
## programming languages
- c++ 17 is required.
- python 3.12 is required.
## setting up directories
1. clone the repo into a directory on your computer.
2. create a folder named `data` in the project root.
3. put the `edges_bboxed.csv` and `nodes_bboxed.csv` files into the `data` dir.
4. create a folder named `db` in the project root.
5. create a folder named `dist` in the project root. This is where you should direct your build system to put compiled binaries.
## dependencies
- the Python `tqdm` package is used to display loading bars on the script that builds the database, it will need to be installed with `pip install tqdm`
- You will need to install SFML so that `#include <SFML/Graphics.hpp>` etc.. works.
- sqlite3 should be installed on your system so that sqlite_orm works (sqlite_orm needs to be able to do `#include <sqlite3.h>`).
- Configure your build tool to also include the packages in the include/ directory, the header files for tomlplusplus and sqlite_orm live here.
- When compiling, you need to link all of the required libraries. Example build command: `gcc -std=c++17 -g src/*.cpp -o dist/app.out -lsfml-graphics -lsfml-window -lsfml-system -lsqlite3 -Iinclude/`. This command will link all of the required SFML components, link sqlite3, and add all of the packages in the `include/` directory.
## creating the database
- Below is an exmaple Makefile that defines tasks for building the DB. Since the CSV data linked above is already filtered to Florida only, you do not need to run the `filter_csv` command.
1. Create the db by running the `create_db` make command
2. Populate the db by running the `fill_db` command (make take ~15 minutes)
3. Remove unused edges and nodes by running the `clean_db` command, this removes bike and walking paths from the database (only car accessible roads are used)
- after these steps, the db directory should contain a sqlite database that is ready to use by the app.
```Makefile
osm4routing: # converts pbf to nodes.csv and edges.csv
	cd ./data && osm4routing us-south-latest.osm.pbf

filter_csv: # filter nodes/edges to the lat/lon bbox defined in the py file
	python ./dev/scripts/filter_to_bbox.py

create_db:
	python ./dev/scripts/create_db.py

fill_db:  # fill the database with node/edge data from the CSV's defined in the populate_db.py file
	python ./dev/scripts/populate_db.py

clean_db:
	python ./dev/scripts/cleanup_db.py
```

# Technical diagrams
## chunking and viewport
![chunk model](https://github.com/Rebeljah/osm_router/assets/3146309/991d91f5-b810-4cb7-9976-053a03d752e6)
![viewport model](https://github.com/Rebeljah/osm_router/assets/3146309/62c84eba-9382-4cf2-8678-8b8c91d987a1)
![viewport buffer](https://github.com/Rebeljah/osm_router/assets/3146309/683ff1c9-6524-4269-ba13-34fe7f740f8f)

## coordinate systems
![displaycoordmodel](https://github.com/Rebeljah/osm_router/assets/3146309/2b8e717b-9777-4741-b403-ebc93b0e4f5a)
![geocoordmodel](https://github.com/Rebeljah/osm_router/assets/3146309/43dcceca-91a2-4602-aa05-d6a2f743505f)
