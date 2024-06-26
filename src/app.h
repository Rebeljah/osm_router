#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <sstream>

using std::string;
using std::vector;
using std::unordered_map;
using std::unordered_set;
using std::stringstream;

#include <SFML/Graphics.hpp>
#include <tomlplusplus/toml.hpp>
#include <lrucache.hpp>

#include "sql.h"

typedef double Degrees;
typedef double Pixels;
typedef double Meters;
typedef int NodeID;
typedef int ChunkID;

// https://gist.github.com/tcmug/9712f9192571c5fe65c362e6e86266f8
vector<string> splitString(string s, string delim) {
    vector<string> result;
    size_t from = 0, to = 0;
    while (string::npos != (to = s.find(delim, from))) {
        result.push_back(s.substr(from, to - from));
        from = to + delim.length();
    }
    result.push_back(s.substr(from, to));
    return result;
}

Meters degreesToMeters(Degrees x)
{
    return x * 110773;
}

Degrees metersToDegrees(Meters x)
{
    return x / 110773;
}

/*
Convert decimal degrees to pixels given the conversion ratio.

let p and d such that p px = d deg. Therefore, 1 deg = (p/d) px, and
x deg = x * (p/d) px.

@param x: input decimal degrees value
@param pd: ratio of pixels per degree
*/
Pixels degreesToPixels(Degrees x, double pd)
{
    return x * pd;
}

/*
Convert pixels to decimal degrees given the terms of the conversion ratio.

let d and p such that p px = d deg. Therefore, 1 px = (d/p) deg and
x px = x * (d/p) deg

@param x: input pixel value
@param dp: ratio of degrees per pixel
*/
Degrees pixelsToDegrees(Pixels x, double dp)
{
    return x * dp;
}

/*
Convert a geo rect in decimal degrees to a pixel rect given a conversion ratio.
@param r: input rect
@param pd: ratio of pixels per degree
*/
DisplayRect geoRectToDisplayRect(GeoRect r, double pd)
{
    auto f = [pd](Pixels x){ return degreesToPixels(x, pd);};
    return DisplayRect(f(r.top), f(r.left), f(r.width), f(r.height));
}

/*
Convert a display rect in pixels to a georect in decimal degrees given a conversion ratio.
@param r: input rect
@param dp: ratio of degrees per pixel
*/
GeoRect displayRectToGeoRect(DisplayRect r, double dp)
{
    auto f = [dp](Pixels x){ return pixelsToDegrees(x, dp);};
    return GeoRect(f(r.top), f(r.left), f(r.width), f(r.height));
}

struct DisplayRect : public sf::Rect<Pixels>
{
    DisplayRect(Pixels top, Pixels left, Pixels w, Pixels h) : sf::Rect<Pixels>(top, left, w, h) {}
    DisplayRect() : sf::Rect<Pixels>(0,0,0,0) {}
};

struct GeoRect : public sf::Rect<Degrees>
{
    GeoRect(Degrees top, Degrees left, Degrees w, Degrees h) : sf::Rect<Degrees>(top, left, w, h) {}
    GeoRect() : sf::Rect<Degrees>(0,0,0,0) {}
};

struct PointPath
{
    vector<sf::Vector2<double>> points;

    PointPath(string wktLinestring)
    {
        for (auto part : splitString(wktLinestring, ", "))
        {
            auto lonLat = splitString(part, " ");
            auto lon = stod(lonLat.at(0));
            auto lat = stod(lonLat.at(1));
            points.push_back(sf::Vector2<double>(lon, lat));
        }
    }

    PointPath() {}
};

struct Edge
{
    sql::Edge data;
    PointPath path;

    Edge(sql::Edge data) : data(data), path(data.wktLinestringOffset)
    {
    }
};

struct Node
{
    sql::Node data;
    vector<Edge> edgesOut;

    Node(sql::Node data) : data(data) {}
};

struct Chunk
{
    int id;
    int gridRow;
    int gridcol;
    GeoRect bbox;
    unordered_map<NodeID, Node> nodes;

    Chunk(sql::Chunk chunk, sql::Storage *storage)
    {
        this->id = chunk.id;
        this->gridRow = chunk.gridRow;
        this->gridcol = chunk.gridCol;
        this->bbox = GeoRect(chunk.topOffset, chunk.leftOffset, chunk.size, chunk.size);

        // prealloc space for all nodes
        nodes.reserve(chunk.nNodes);

        using namespace sqlite_orm;

        // load all nodes and attach out-edges to them

        for (auto sqlNode : storage->iterate<sql::Node>(where(c(&sql::Node::chunk) == chunk.id), limit(chunk.nNodes)))
        {
            nodes.emplace(sqlNode.id, Node(sqlNode));
        }

        for (auto sqlEdge : storage->iterate<sql::Edge>(where(c(&sql::Edge::chunk) == chunk.id), limit(chunk.nEdges)))
        {
            nodes.at(sqlEdge.sourceNode).edgesOut.push_back(Edge(sqlEdge));
        }
    }
};

class ChunkCache
{
public:
    ChunkCache(sql::Storage *storage) : storage(storage), cache(int(512 / 0.112764))  // limit to 512mb, average chunksize is 0.112764mb these values are in the config also
    {

    }

    Chunk *get(int row, int col)
    {
        using namespace sqlite_orm;

        // convert row,col to string because keys need to be string
        stringstream ss;
        ss << row << col;
        auto key = ss.str();

        // load chunk if there is a cache miss
        if (!cache.exists(key))
        {
            auto sqlChunk = storage->get_all<sql::Chunk>(where(c(&sql::Chunk::gridRow) == row and c(&sql::Chunk::gridCol) == col), limit(1)).at(0);
            cache.put(key, new Chunk(sqlChunk, storage));
        }

        return cache.get(key);
    }
    
private:
    cache::lru_cache<string, Chunk *> cache;
    sql::Storage *storage;
};

class MapSprite : public sf::Sprite
{
public:
    MapSprite()
    {  
    }
    MapSprite(GeoRect mapAreaGeoBbox, Pixels viewportW, Pixels viewPortH, double ratioPixelsPerDegree, Degrees chunkSize)
    {
        double dp = 1/ratioPixelsPerDegree;  // degrees per pixel

        this->ratioPixelsPerDegree = ratioPixelsPerDegree;
        this->chunkSize = chunkSize;

        // geographic bounding boxes for entire map and visible area
        this->mapAreaGeoBbox = mapAreaGeoBbox;
        this->viewportGeoBbox = GeoRect(0, 0, pixelsToDegrees(viewportW, dp), pixelsToDegrees(viewPortH, dp));

        // create render texture that is the same size as the map area in pixels
        // all drawing will be done to this target so that off-screen drawing can be done.
        auto rect = geoRectToDisplayRect(mapAreaGeoBbox, ratioPixelsPerDegree);
        this->renderTexture.create(static_cast<float>(rect.width), static_cast<float>(rect.height));

        // set this's Texture to that of the render texture so that this can be draw to window
        // To change the visible region of the map, use this->setTextureRect to
        // bound the correct region of the RenderTexture
        this->setTexture(this->renderTexture.getTexture());
    }

    void renderChunk(const Chunk &chunk)
    {
        // skip if chunk is already drawn
        if (rendered_chunks.find(chunk.id) != rendered_chunks.end())
            return;
        
        // TODO maybe modify DB so that lat/lon is stored as offset from area topleft lat/lon ?
        // because the locations of nodes and edges are static, this would save a lot of calculation.

        for (const auto &kv : chunk.nodes)
        {
            for (const auto &edge : kv.second.edgesOut)
            {

            }
        }

        rendered_chunks.emplace(chunk.id);
    }

private:
    sf::RenderTexture renderTexture;

    unordered_set<ChunkID> rendered_chunks;
    Degrees chunkSize;

    GeoRect mapAreaGeoBbox;
    GeoRect viewportGeoBbox;
    double ratioPixelsPerDegree;

};

class App
{
public:
    App() : window(sf::VideoMode(800, 800), "Map")
    {
        window.setFramerateLimit(*config["graphics"]["framerate"].value<int>());
    }

    ~App()
    {
    }

    void run()
    {
        while (window.isOpen())
        {
            processEvents();
            update();
            render();
        }
    }

private:
    sf::RenderWindow window;
    MapSprite mapSprite;

    toml::v3::ex::parse_result config = toml::parse_file("./config/config.toml");
    sql::Storage storage = sql::loadStorage("./db/map.db");

    void processEvents()
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
                window.close();
        }
    }

    void update()
    {
    }

    void render()
    {
        window.clear();
        window.draw(mapSprite);
        window.display();
    }
};
