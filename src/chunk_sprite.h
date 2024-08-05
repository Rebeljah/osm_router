#pragma once

#include <unordered_map>
#include <queue>

#include <SFML/Graphics.hpp>

#include <vector>
#include <thread>
#include <mutex>
#include <unordered_set>
#include <string>

#include "sql.h"
#include "geometry.h"
#include "node.h"

using std::mutex;
using std::pair;
using std::queue;
using std::string;
using std::thread;
using std::unordered_map;
using std::unordered_set;
using std::vector;

string chunkId(int row, int col)
{
    return std::to_string(row) + ',' + std::to_string(col);
}

struct Chunk
{
    sql::Chunk data;
    Rectangle<float> geoRect;
    unordered_map<int, Node> nodes;

    Chunk(sql::Chunk chunk, sql::Storage *storage)
    {
        this->data = chunk;

        // prealloc space for all nodes
        nodes.reserve(chunk.numNodes);

        using namespace sqlite_orm;

        // load all nodes in the chunk from the db
        for (auto sqlNode : storage->iterate<sql::Node>(where(c(&sql::Node::chunkId) == chunk.id), limit(chunk.numNodes)))
        {
            nodes.emplace(sqlNode.id, Node(sqlNode));
        }

        // load all edges in the chunk from the db and push them to nodes
        for (auto sqlEdge : storage->iterate<sql::Edge>(where(c(&sql::Edge::chunkId) == chunk.id), limit(chunk.numEdges)))
        {
            nodes.at(sqlEdge.sourceNodeId).edgesOut.push_back(Edge(sqlEdge));
        }
    }
};

class ChunkLoader
{
public:
    ~ChunkLoader()
    {
        // signal for all worker threads to stop, then wait for them all to finish
        m_stopWorkers = true;
        for (std::thread *worker : m_workerThreads)
        {
            worker->join();
        }

        // delete all of the chunks in the cache
        for (vector<Chunk *> &row : m_cache)
        {
            for (Chunk *p : row)
            {
                delete p;
            }
        }
    }

    void start(string dbFilePath)
    {
        // this flag will be set to true to stop the workers
        m_stopWorkers = false;
        // start threads that load chunks from the db so that
        // chunks can be loaded in the background without freezing the app
        auto workerTask = [this, dbFilePath]()
        {
            this->workerThread(dbFilePath);
        };

        // threads need to be stored on the heap. If they were on the stack, they
        // would get deleted immediately
        for (int i = 0; i < 5; ++i)
        {
            m_workerThreads.push_back(new std::thread(workerTask));
        }
    };

    /*
    If the chunk is already loaded, immedaitely returns a pointer to the chunk,
    if it is not yet loaded, start loading in a separate thread and return a
    null option

    @param row: row of the chunk to load
    @param col: col of the chunk to load
    @returns An empty option if the chunk is not loaded, else an option containing
     a pointer to the cached chunk.
    */
    std::optional<Chunk *> get(int row, int col)
    {
        // resize the cache grid and isLoading flag grid to fit the chunk
        if (m_cache.size() <= row)
        {
            m_cache.resize(row + 1);
            m_isLoading.resize(row + 1);
        }
        if (m_cache[row].size() <= col)
        {
            m_cache[row].resize(col + 1, nullptr);   // cache slots are init'd as nullptr
            m_isLoading[row].resize(col + 1, false); // flag grid slots are init'd false
        }

        // check if chunk is cached first
        Chunk *pChunk = m_cache[row][col];

        // cache miss, so start loading and return a null option for now
        if (pChunk == nullptr)
        {
            startLoadingChunk(row, col);
            return std::nullopt;
        }

        return pChunk;
    }

    void unCache(int row, int col)
    {
        delete m_cache[row][col];
        m_cache[row][col] = nullptr;
    }

private:
    void startLoadingChunk(int row, int col)
    {
        // if the chunk is not already loading, push it to the queue so that
        // the loader thread will retrieve it from the db

        // a mutex is used to limit access of shared state to one thread at a time
        m_mutex.lock();
        {
            if (!m_isLoading[row][col])
            {
                m_loadQueue.push({row, col});
                m_isLoading[row][col] = true;
            }
        }
        m_mutex.unlock();
    }

    void workerThread(string dbFilePath)
    {
        using namespace sqlite_orm;

        // https://www.sqlite.org/threadsafe.html
        sqlite3_config(SQLITE_CONFIG_MULTITHREAD);

        // each worker thread gets its own connection to the db
        auto storage = sql::loadStorage(dbFilePath);

        // continually take coordinates of chunks to be loaded from the work queue
        while (!m_stopWorkers)
        {
            m_mutex.lock();
            if (m_loadQueue.empty()) // no work to do yet
            {
                m_mutex.unlock();
                // wait a bit before checking for work so that the mutex isn't hogged
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            auto [row, col] = m_loadQueue.front();
            m_loadQueue.pop();

            m_mutex.unlock();

            // load chunk sql data then init Chunk with data
            // the Chunk constructor needs the storage object because it will
            // load all of the nodes and edges that are inside of it.
            sql::Chunk data = storage.get<sql::Chunk>(chunkId(row, col));
            Chunk *newChunk = new Chunk(data, &storage);

            // place the chunk into the cache and unmark it as loading
            m_mutex.lock();
            {
                m_cache[row][col] = newChunk;
                m_isLoading[row][col] = false;
            }
            m_mutex.unlock();
        }
    }

    vector<vector<Chunk *>> m_cache;
    vector<vector<bool>> m_isLoading;
    queue<pair<int, int>> m_loadQueue;
    vector<thread *> m_workerThreads;
    mutex m_mutex;
    bool m_stopWorkers;
};

struct ChunkSprite : sf::Sprite
{
    ChunkSprite(Rectangle<double> rect, int row, int col) : rect(rect), row(row), col(col)
    {
        // Drawing will be done on a RenderTexture so that it can be cached
        // this avoid redrawing all of the roads on each frame
        renderTexture.create(rect.width + 1, rect.height + 1);
        this->setTexture(renderTexture.getTexture());
    }

    void renderEdge(Edge &edge, MapGeometry *mapGeometry)
    {
        // the edges path is rendered by loading it's points into a vertex array
        // then drawing the vertex array to the RenderTexture.
        // the edge path points are loaded as offset lon, lat coordinates,
        // (if a point is at the topleft of the map area, then it's coordinates will be 0,0).
        sf::VertexArray path(sf::LineStrip, edge.path.points.size());
        for (int i = 0; i < edge.path.points.size(); ++i)
        {
            // convert the offset lon, lat to a map-relative pixel coordinate
            auto pointDisplayCoordinate = mapGeometry->toPixelVector(edge.path.points[i]);
            // offset the coordinate to the RenderTexture display rectangle
            pointDisplayCoordinate -= {rect.left, rect.top};

            path[i].color = edge.color;
            path[i].position = sf::Vector2f(pointDisplayCoordinate);
        }
        // draw the completed path
        renderTexture.draw(path);
    }

    void renderDot(sf::Vector2<double> geoCoordinate, MapGeometry *mapGeometry)
    {
        hasDots = true;
        auto position = mapGeometry->toPixelVector(geoCoordinate);
        position -= {rect.left, rect.top};
        sf::Vertex point(sf::Vector2f(position), sf::Color::Red);
        renderTexture.draw(&point, 1, sf::Points);
        renderTexture.display();
    }

    sf::RenderTexture renderTexture;
    Rectangle<double> rect;
    bool hasDots = false;
    int row;
    int col;
};

class ChunkSpriteLoader
{
public:
    void init(MapGeometry *mapGeometry, std::string dbFilePath)
    {
        m_pMapGeometry = mapGeometry;
        chunkLoader.start(dbFilePath);
    }

    std::optional<ChunkSprite *> get(int row, int col)
    {
        // grow the cache grid to fit the new sprite if needed
        if (m_grid.size() <= row)
            m_grid.resize(row + 1);
        if (m_grid[row].size() <= col)
            m_grid[row].resize(col + 1);

        // return the sprite if already loaded and in the cache
        if (m_grid[row][col] != nullptr)
        {
            return m_grid[row][col];
        }

        // return null option if the chunk is not loaded yet
        std::optional<Chunk *> chunkOpt;
        if (!(chunkOpt = chunkLoader.get(row, col)).has_value())
        {
            return std::nullopt;
        }

        // chunk is loaded, so it can be used to render sprite
        renderChunkSprite(**chunkOpt, row, col);
        renderInterchunkEdges();
        // chunkLoader.unCache(row, col);

        m_grid[row][col]->renderTexture.display();
        return m_grid[row][col];
    }

    bool has(int row, int col)
    {
        return m_grid.size() > row && m_grid[row].size() > col && m_grid[row][col] != nullptr;
    }

    void unCache(int row, int col)
    {
        delete m_grid[row][col];
        m_grid[row][col] = nullptr;
    }

    std::vector<ChunkSprite *> getAllLoaded()
    {
        std::vector<ChunkSprite *> res;
        for (auto &row : m_grid)
        {
            for (ChunkSprite *sprite : row)
            {
                if (sprite != nullptr)
                    res.push_back(sprite);
            }
        }

        return res;
    }

private:
    void renderChunkSprite(Chunk &chunk, int row, int col)
    {
        auto rect = m_pMapGeometry->toPixelRectangle(
            {chunk.data.offsetLatTop,
             chunk.data.offsetLonLeft,
             m_pMapGeometry->getChunkGeoSize(),
             m_pMapGeometry->getChunkGeoSize()});

        auto chunkSprite = new ChunkSprite(rect, row, col);

        // render all edges in the chunk onto the chunkSprite texture
        for (auto &[_, node] : chunk.nodes)
        {
            for (auto &edge : node.edgesOut)
            {
                chunkSprite->renderEdge(edge, m_pMapGeometry);

                // find edges that cross into other chunks
                // This work by getting a bounding box that encompasses the edge,
                // if the bounding box overlaps other chunks, these will be added
                // to the map and drawn onto the other chunk/s on the next frame.
                Rectangle<double> edgeGeoBbox = edge.path.getGeoBoundingBox();
                // get the range of chunks coordinates (r0 to r1, c0 to c1) that the
                // edge's bounding box intersects
                auto overlap = m_pMapGeometry->calculateOverlappingChunks(edgeGeoBbox);

                for (int r = overlap.top; r <= overlap.bottom(); ++r)
                {
                    for (int c = overlap.left; c <= overlap.right(); ++c)
                    {
                        if (r == chunk.data.row && c == chunk.data.col)
                            continue; // don't render edge again on current chunk

                        interChunkEdges.push(std::make_pair(std::make_pair(r, c), edge));
                    }
                }
            }
        }

        // cache the sprite
        m_grid[row][col] = chunkSprite;
    }

    void renderInterchunkEdges()
    {
        // cycle through all edges on the queue and render them onto
        // the chunks that they overlap if the chunk is loaded.
        int n_in_queue = interChunkEdges.size();
        for (int i = 0; i < n_in_queue; ++i)
        {
            // pop the next chunk cache coordinate and edge
            auto item = interChunkEdges.front();
            auto [coord, edge] = item;
            auto [row, col] = coord;
            interChunkEdges.pop();

            // if the cache grid doesn't have the sprite loaded, push the work back onto queue for later
            if (row >= m_grid.size() || col >= m_grid[row].size() || m_grid[row][col] == nullptr)
            {
                // the chunk is not loaded yet, so push the work back onto the queue for later
                interChunkEdges.push(item);
                continue;
            }

            // chunk loaded, so the interchunk edge can be rendered onto it.
            m_grid[row][col]->renderEdge(edge, m_pMapGeometry);
        }
    }

    queue<pair<pair<int, int>, Edge>> interChunkEdges;
    vector<vector<ChunkSprite *>> m_grid;
    ChunkLoader chunkLoader;
    MapGeometry *m_pMapGeometry;
};