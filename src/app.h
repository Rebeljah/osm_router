#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <thread>
#include <future>
#include <queue>
#include <chrono>
#include <optional>

using std::string;
using std::unordered_map;
using std::vector;
using std::promise;
using std::future;

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
vector<string> splitString(string s, string delim)
{
    vector<string> result;
    size_t from = 0, to = 0;
    while (string::npos != (to = s.find(delim, from)))
    {
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
    DisplayRect() : sf::Rect<Pixels>(0, 0, 0, 0) {}
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
    GeoRect() : sf::Rect<Degrees>(0, 0, 0, 0) {}
};

/*
Convert a geo rect in decimal degrees to a pixel rect given a conversion ratio.
@param r: input rect
@param pd: ratio of pixels per degree
*/
DisplayRect geoRectToDisplayRect(GeoRect r, double pd)
{
    auto f = [pd](Pixels x)
    { return degreesToPixels(x, pd); };
    return DisplayRect(f(r.top), f(r.left), f(r.width), f(r.height));
}

/*
Convert a display rect in pixels to a georect in decimal degrees given a conversion ratio.
@param r: input rect
@param dp: ratio of degrees per pixel
*/
GeoRect displayRectToGeoRect(DisplayRect r, double dp)
{
    auto f = [dp](Pixels x)
    { return pixelsToDegrees(x, dp); };
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

class ChunkLoader
{
public:
    void start(sql::Storage *storage)
    {
        t1 = std::thread([this, storage](){ this->loaderThread(storage); });
    };

    std::optional<Chunk *> get(int row, int col)
    {
        if (m_chunkGrid.size() <= row)
            m_chunkGrid.resize(row + 1);

        if (m_chunkGrid[row].size() <= col)
            m_chunkGrid[row].resize(col + 1);

        if (auto pChnk = m_chunkGrid[row][col])
        {
            return pChnk;
        }

        load(row, col);
        return std::nullopt;
    }
private:
    void load(int row, int col)
    {
        auto gridCoord = std::make_pair(row, col);

        m_mutex.lock();

        bool alreadyLoading = !m_currentlyLoading.emplace(gridCoord).second;
        if (!alreadyLoading)
        {
            m_loadQueue.push(gridCoord);
        }

        m_mutex.unlock();
    }

    void loaderThread(sql::Storage *storage)
    {
        using namespace sqlite_orm;

        while (true)
        {
            if (m_loadQueue.empty())
            {
                continue;
            }

            m_mutex.lock();

            auto gridCoord = m_loadQueue.front();
            m_loadQueue.pop();
            
            m_mutex.unlock();

            auto [row, col] = gridCoord;
            auto sqlChunk = storage->get_all<sql::Chunk>(
                where(
                    c(&sql::Chunk::gridRow) == row and
                    c(&sql::Chunk::gridCol) == col
                ),
                limit(1)
            ).at(0);

            Chunk *newChunk = new Chunk(sqlChunk, storage);

            m_mutex.lock();

            m_chunkGrid[row][col] = newChunk;
            m_currentlyLoading.erase(gridCoord);

            m_mutex.unlock();
        }
    }

    vector<vector<Chunk *>> m_chunkGrid;
    std::mutex m_mutex;
    std::queue<std::pair<int, int>> m_loadQueue;
    std::unordered_set<std::pair<int, int>, PairHasher<int>> m_currentlyLoading;
    std::thread t1;
};

struct ChunkSprite : sf::Sprite
{
    ChunkSprite(DisplayRect rect, const Chunk &chunk, double pd) : rect(rect)
    {
        renderTexture.create(rect.width+1, rect.height+1);
        for (auto [id, node] : chunk.nodes)
        {
            Degrees offsetLat = node.data.offsetLatitude;
            Degrees offsetLon = node.data.offsetLongitude;
            Pixels x = degreesToPixels(offsetLon, pd) - rect.left;
            Pixels y = degreesToPixels(offsetLat, pd) - rect.top;

            auto dot = sf::CircleShape(1, 3);
            dot.setFillColor(sf::Color::Green);
            dot.setPosition({x, y});
            renderTexture.draw(dot);
        }
        renderTexture.display();
        this->setTexture(renderTexture.getTexture());
    }

    sf::RenderTexture renderTexture;
    DisplayRect rect;
};

class ChunkSpriteLoader
{
public:
    ChunkSpriteLoader() {}

    ChunkSpriteLoader(ChunkLoader *pChunkLoader, Pixels chunkSize, double pd)
    : m_pChunkLoader(pChunkLoader),
      m_chunkSize(chunkSize),
      m_pd(pd)
    {
    }

    void init(ChunkLoader *pChunkLoader, Pixels chunkSize, double pd)
    {
        m_pChunkLoader = pChunkLoader;
        m_chunkSize = chunkSize;
        m_pd = pd;
    }

    std::optional<ChunkSprite *> get(int row, int col)
    {
        ChunkSprite *res = nullptr;

        if (m_grid.size() <= row)
            m_grid.resize(row + 1);

        if (m_grid[row].size() <= col)
            m_grid[row].resize(col + 1);
        
        if (res = m_grid[row][col])
        {
            return res;
        }

        std::optional<Chunk *> chunk;
        if (!(chunk = m_pChunkLoader->get(row, col)).has_value())
        {
            return std::nullopt;
        }

        DisplayRect rect = DisplayRect(degreesToPixels((*chunk)->data.topOffset, m_pd), degreesToPixels((*chunk)->data.leftOffset, m_pd), m_chunkSize, m_chunkSize);
        res = new ChunkSprite(rect, **chunk, m_pd);
        m_grid[row][col] = res;

        return res;
    }

private:
    vector<vector<ChunkSprite *>> m_grid;
    ChunkLoader *m_pChunkLoader;
    Pixels m_chunkSize;
    double m_pd;
};

class Viewport : public DisplayRect
{
public:
    Viewport() {}

    Viewport(Pixels w, Pixels h, Pixels mapWidth, Pixels mapHeight) : DisplayRect(0, 0, w, h)
    {
        this->mapBounds = DisplayRect(0, 0, mapWidth, mapHeight);
    }

    Viewport &operator=(const Viewport &other)
    {
        this->top = other.top;
        this->left = other.left;
        this->width = other.width;
        this->height = other.height;
        this->mapBounds = other.mapBounds;
        return *this;
    }

    void controlPanning(PanDirection direction, bool isPanning)
    {
        switch (direction)
        {
        case PanDirection::Up:
            isPanningUp = isPanning;
            break;
        case PanDirection::Down:
            isPanningDown = isPanning;
            break;
        case PanDirection::Left:
            isPanningLeft = isPanning;
            break;
        case PanDirection::Right:
            isPanningRight = isPanning;
            break;
        }
    }

    void update(float deltaTime)
    {
        double delta = panVelocity * deltaTime;
        bool panned = false;

        if (isPanningUp ^ isPanningDown) // one or the other but not both
        {
            if (isPanningUp)
            {
                top -= delta;
            }

            if (isPanningDown)
            {
                top += delta;
            }

            panned = true;
        }

        if (isPanningLeft ^ isPanningRight) // stops from trying to pan left and right at same time
        {
            if (isPanningLeft)
            {
                left -= delta;
            }

            if (isPanningRight)
            {
                left += delta;
            }

            panned = true;
        }

        // check collision with map boundaries
        if (left < mapBounds.left)
        {
            left = mapBounds.left;
        }
        else if (right() > mapBounds.right())
        {
            left = mapBounds.right() - width;
        }
        if (top < mapBounds.top)
        {
            top = mapBounds.top;
        }
        else if (bottom() > mapBounds.bottom())
        {
            top = mapBounds.bottom() - height;
        }
    }

private:
    Pixels chunkSize;
    DisplayRect mapBounds;

    // panning controls
    int panVelocity = 450; // pixels per second
    bool isPanningLeft = false;
    bool isPanningRight = false;
    bool isPanningUp = false;
    bool isPanningDown = false;
};

class App
{
public:
    App() : window(sf::VideoMode(800, 800), "OSM Router"), chunkSpriteLoader()
    {
        Degrees mapTop = *config["map"]["bbox_top"].value<double>();
        Degrees mapLeft = *config["map"]["bbox_left"].value<double>();
        Degrees mapBottom = *config["map"]["bbox_bottom"].value<double>();
        Degrees mapRight = *config["map"]["bbox_right"].value<double>();
        Degrees viewportW = *config["viewport"]["default_w"].value<double>();
        Degrees viewportH = *config["viewport"]["default_h"].value<double>();
        Degrees chunkSize = *config["map"]["chunk_size"].value<double>();

        pd = 800 / viewportW;

        viewport = Viewport(
            degreesToPixels(viewportW, pd),
            degreesToPixels(viewportH, pd),
            degreesToPixels(mapRight - mapLeft, pd),
            degreesToPixels(mapTop - mapBottom, pd));
        
        chunkLoader.start(&storage);
        chunkSpriteLoader.init(&chunkLoader, degreesToPixels(chunkSize, pd), pd);

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
                    viewport.controlPanning(PanDirection::Up, wasPressed);
                }
                else if (event.key.code == sf::Keyboard::Key::Down)
                {
                    viewport.controlPanning(PanDirection::Down, wasPressed);
                }
                else if (event.key.code == sf::Keyboard::Key::Left)
                {
                    viewport.controlPanning(PanDirection::Left, wasPressed);
                }
                else if (event.key.code == sf::Keyboard::Key::Right)
                {
                    viewport.controlPanning(PanDirection::Right, wasPressed);
                }
            }
        }
    }

    void update()
    {
        float deltaTime = clock.restart().asSeconds();
        viewport.update(deltaTime);
    }

    void render()
    {
        window.clear();

        Pixels chunkSize = degreesToPixels(*config["map"]["chunk_size"].value<double>(), pd);
        int chunkRowTop = int(viewport.top / chunkSize);
        int chunkRowBottom = int(viewport.bottom() / chunkSize);
        int chunkColLeft = int(viewport.left / chunkSize);
        int chunkColRight = int(viewport.right() / chunkSize);

        for (int row = chunkRowTop - 1; row <= chunkRowBottom + 1; ++row)
        {
            for (int col = chunkColLeft - 1; col <= chunkColRight + 1; ++col)
            {
                // TODO check right and bottom bound also
                if (row < 0 || col < 0)
                {
                    continue;
                }

                auto sprite = chunkSpriteLoader.get(row, col);

                if (!sprite.has_value())
                    continue;

                // skip drawing chunks that are buffered but not in the viewport
                if (row < chunkRowTop || row > chunkRowBottom || col < chunkColLeft || col > chunkColRight)
                    continue;

                // draw chunk
                (*sprite)->setPosition((*sprite)->rect.left - viewport.left, (*sprite)->rect.top - viewport.top);
                window.draw(**sprite);
            }
        }

        window.display();
    }

    sf::RenderWindow window;
    sf::Clock clock;

    Viewport viewport;
    ChunkLoader chunkLoader;
    ChunkSpriteLoader chunkSpriteLoader;
    double pd;

    toml::v3::ex::parse_result config = toml::parse_file("./config/config.toml");
    sql::Storage storage = sql::loadStorage("./db/map.db");
};