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

enum class PanDirection
{
    Up,
    Down,
    Left,
    Right,
};


// code adapted from: https://www.geeksforgeeks.org/unordered-set-of-pairs-in-c-with-examples/#
template <typename T>
struct PairHasher
{
    size_t operator()(const std::pair<T, T> &pair) const
    {
        return std::hash<T>{}(pair.first) ^ std::hash<T>{}(pair.second);
    }
};

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

struct DisplayRect : public sf::Rect<Pixels>
{
    DisplayRect(Pixels top, Pixels left, Pixels w, Pixels h) : sf::Rect<Pixels>(left, top, w, h) {}
    DisplayRect() : sf::Rect<Pixels>(0,0,0,0) {}
    Pixels right() const
    {
        return left + width;
    }
    Pixels bottom() const
    {
        return top + height;
    }
};

struct GeoRect : public sf::Rect<Degrees>
{
    GeoRect(Degrees top, Degrees left, Degrees w, Degrees h) : sf::Rect<Degrees>(left, top, w, h) {}
    GeoRect() : sf::Rect<Degrees>(0,0,0,0) {}
};

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
    sql::Chunk data;
    GeoRect geoBbox;
    unordered_map<NodeID, Node> nodes;

    Chunk(sql::Chunk chunk, sql::Storage *storage)
    {
        this->data = chunk;
        this->geoBbox = GeoRect(chunk.topOffset, chunk.leftOffset, chunk.size, chunk.size);

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

    ChunkCache() : cache(0) {}

    Chunk *get(int row, int col)
    {
        using namespace sqlite_orm;

        // convert row,col to string because keys need to be string
        stringstream ss;
        ss << row << col;
        auto key = ss.str();

        Chunk *pChunk;

        try 
        {
            pChunk = cache.get(key);
        }
        catch (std::range_error)
        {
            sql::Chunk sqlChunk = storage->get_all<sql::Chunk>(where(c(&sql::Chunk::gridRow) == row and c(&sql::Chunk::gridCol) == col), limit(1)).at(0);
            pChunk = new Chunk(sqlChunk, storage);
            cache.put(key, pChunk);
        }

        return pChunk;
    }
    
private:
    cache::lru_cache<string, Chunk *> cache;
    sql::Storage *storage;
};

struct ChunkSprite : sf::Sprite
{
    ChunkSprite(DisplayRect rect, const Chunk &chunk, double pd) : rect(rect)
    {
        renderTexture.create(rect.width, rect.height);
        for (auto [ id, node ] : chunk.nodes)
        {
            Degrees offsetLat = node.data.offsetLatitude;
            Degrees offsetLon = node.data.offsetLongitude;
            Pixels x = degreesToPixels(offsetLon, pd) - rect.left;
            Pixels y = degreesToPixels(offsetLat, pd) - rect.top;

            auto dot = sf::CircleShape(2, 4);
            dot.setFillColor(sf::Color::Green);
            dot.setPosition({ x, y });
            renderTexture.draw(dot);
        }
        renderTexture.display();
        this->setTexture(renderTexture.getTexture());
    }

    sf::RenderTexture renderTexture;
    DisplayRect rect;
};

class Viewport
{
public:
    Viewport()
    {  
    }
    Viewport(Pixels w, Pixels h, double ratioPixelsPerDegree, Pixels chunkSize, ChunkCache *chunkCache, DisplayRect mapBounds)
    {
        this->mapBounds = mapBounds;
        this->visibleArea = DisplayRect(mapBounds.top, mapBounds.left, w, h);
        this->ratioPixelsPerDegree = ratioPixelsPerDegree;
        this->chunkSize = chunkSize;
        this->chunkCache = chunkCache;
    }

    void controlPanning(PanDirection direction, bool isPanning)
    {
        switch (direction)
        {
            case PanDirection::Up:
                isPanningUp = isPanning; break;
            case PanDirection::Down:
                isPanningDown = isPanning; break;
            case PanDirection::Left:
                isPanningLeft = isPanning; break;
            case PanDirection::Right:
                isPanningRight = isPanning; break;
        }
    }

    void update(float deltaTime)
    {
        double delta = panVelocity * deltaTime;
        bool panned = false;

        if (isPanningUp ^ isPanningDown)  // one or the other but not both
        {
            if (isPanningUp)
            {
                visibleArea.top -= delta;
            }

            if (isPanningDown)
            {
                visibleArea.top += delta;
            }

            panned = true;
        }

        if (isPanningLeft ^ isPanningRight)  // stops from trying to pan left and right at same time
        {
            if (isPanningLeft)
            {
                visibleArea.left -= delta;
            }

            if (isPanningRight)
            {
                visibleArea.left += delta;
            }

            panned = true;
        }

        // check collision with map boundaries
        if (visibleArea.left < mapBounds.left)
        {
            visibleArea.left = mapBounds.left;
        }
        else if (visibleArea.right() > mapBounds.right())
        {
            visibleArea.left = mapBounds.right() - visibleArea.width;
        }
        if (visibleArea.top < mapBounds.top)
        {
            visibleArea.top = mapBounds.top;
        }
        else if (visibleArea.bottom() > mapBounds.bottom())
        {
            visibleArea.top = mapBounds.bottom() - visibleArea.height;
        }
    }

    void render(sf::RenderWindow &window)
    {
        // calculate intersecting chunks by calculating the range of of chunk rows
        // and chunk columns that the viewport intersects with

        int chunkRowTop = int(visibleArea.top / chunkSize);
        int chunkRowBottom = int(visibleArea.bottom() / chunkSize);
        int chunkColLeft = int(visibleArea.left / chunkSize);
        int chunkColRight = int(visibleArea.right() / chunkSize);

        for (int row = chunkRowTop; row <= chunkRowBottom; ++row)
        {
            for (int col = chunkColLeft; col <= chunkColRight; ++col)
            {
                auto key = std::make_pair(row, col);

                ChunkSprite *chunkSprite;

                if (renderedChunks.find(key) == renderedChunks.end())
                {
                    const Chunk *chunk = chunkCache->get(row, col);
                    DisplayRect rect = DisplayRect(degreesToPixels(chunk->data.topOffset, ratioPixelsPerDegree), degreesToPixels(chunk->data.leftOffset, ratioPixelsPerDegree), chunkSize, chunkSize);
                    chunkSprite = new ChunkSprite(rect, *chunk, ratioPixelsPerDegree);
                    renderedChunks.emplace(key, chunkSprite);
                }
                else{
                    chunkSprite = renderedChunks.at(key);
                }

                // draw chunk
                chunkSprite->setPosition(chunkSprite->rect.left - visibleArea.left, chunkSprite->rect.top - visibleArea.top);
                window.draw(*chunkSprite);
            }
        }
    }

private:
    sf::RenderTexture renderTexture;

    ChunkCache *chunkCache;
    unordered_map<std::pair<int, int>, ChunkSprite *, PairHasher<int>> renderedChunks;
    Pixels chunkSize;

    DisplayRect mapBounds;
    DisplayRect visibleArea;
    double ratioPixelsPerDegree;

    //panning controls
    int panVelocity = 300; // pixels per second
    bool isPanningLeft = false;
    bool isPanningRight = false;
    bool isPanningUp = false;
    bool isPanningDown = false;
};

class App
{
public:
    App() : window(sf::VideoMode(800, 800), "Map")
    {
        window.setFramerateLimit(*config["graphics"]["framerate"].value<int>());
        chunkCache = new ChunkCache(&storage);
        
        Degrees mapTop = *config["map"]["bbox_top"].value<double>();
        Degrees mapLeft = *config["map"]["bbox_left"].value<double>();
        Degrees mapBottom = *config["map"]["bbox_bottom"].value<double>();
        Degrees mapRight = *config["map"]["bbox_right"].value<double>();
        Degrees viewportW = *config["viewport"]["default_w"].value<double>();
        Degrees viewportH = *config["viewport"]["default_h"].value<double>();
        Degrees chunkSize = *config["map"]["chunk_size"].value<double>();

        double pd = 800 / viewportW;
        
        viewport = new Viewport(
            degreesToPixels(viewportW, pd),
            degreesToPixels(viewportH, pd),
            pd,
            degreesToPixels(chunkSize, pd),
            chunkCache,
            geoRectToDisplayRect(GeoRect(0, 0, mapRight - mapLeft, mapTop - mapBottom), pd)
        );
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
    sf::Clock clock;

    Viewport *viewport;
    ChunkCache *chunkCache;

    toml::v3::ex::parse_result config = toml::parse_file("./config/config.toml");
    sql::Storage storage = sql::loadStorage("./db/map.db");

    void processEvents()
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
                window.close();
            
            // pan the map around by holding arrow keys
            if (event.type == sf::Event::KeyPressed || event.type == sf::Event::KeyReleased)
            {
                bool wasPressed = event.type == sf::Event::KeyPressed;

                if (event.key.code == sf::Keyboard::Key::Up)
                {
                    viewport->controlPanning(PanDirection::Up, wasPressed);
                }
                else if (event.key.code == sf::Keyboard::Key::Down)
                {
                    viewport->controlPanning(PanDirection::Down, wasPressed);
                }
                else if (event.key.code == sf::Keyboard::Key::Left)
                {
                    viewport->controlPanning(PanDirection::Left, wasPressed);
                }
                else if (event.key.code == sf::Keyboard::Key::Right)
                {
                    viewport->controlPanning(PanDirection::Right, wasPressed);
                }
            }
        }
    }

    void update()
    {
        float deltaTime = clock.restart().asSeconds();
        viewport->update(deltaTime);
    }

    void render()
    {
        window.clear();
        viewport->render(window);
        window.display();
    }
};
