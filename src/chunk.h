#pragma once

#include <unordered_map>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
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

    Chunk(sql::Chunk chunk, float geoSize, sql::Storage *storage)
    {
        this->data = chunk;
        this->geoRect = Rectangle<float>(chunk.offsetLatTop, chunk.offsetLonLeft, geoSize, geoSize);

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
    void start(sql::Storage *storage, float chunkGeoSize)
    {
        // start a thread that loads from the db so that
        // chunks can be loaded in the background without freezing the app
        m_thread = std::thread([this, storage, chunkGeoSize]()
                               { this->loaderThread(storage, chunkGeoSize); });
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
        if (m_chunkGrid.size() <= row)
            m_chunkGrid.resize(row + 1);

        if (m_chunkGrid[row].size() <= col)
            m_chunkGrid[row].resize(col + 1);

        // check if chunk if already loaded
        if (auto pChnk = m_chunkGrid[row][col])
        {
            return pChnk;
        }

        // start loading chunk and return empty option
        load(row, col);
        return std::nullopt;
    }

private:
    void load(int row, int col)
    {
        auto gridCoord = std::make_pair(row, col);
        
        // if the chunk is not already loading, push it to the queue so that
        // the loader thread will retrieve from the db
        m_mutex.lock();
        {
            bool alreadyLoading = !m_currentlyLoading.emplace(gridCoord).second;
            if (!alreadyLoading)
            {
                m_loadQueue.push(gridCoord);
            }
        }
        m_mutex.unlock();
    }

    void loaderThread(sql::Storage *storage, float chunkGeoSize)
    {
        using namespace sqlite_orm;

        // continually take coordinates of chunks to be loaded from the work queue
        while (true)
        {
            if (m_loadQueue.empty()) // no work to do yet
            {
                continue;
            }

            auto gridCoord = m_loadQueue.front();

            m_mutex.lock();
            {
                m_loadQueue.pop();
            }
            m_mutex.unlock();

            // load chunk sql data then init Chunk with data
            // the Chunk constructor needs the storage object because it will
            // load all of the nodes and edges that are inside of it.
            const auto &[row, col] = gridCoord;
            auto sqlChunk = storage->get<sql::Chunk>(chunkId(row, col));
            auto newChunk = new Chunk(sqlChunk, chunkGeoSize, storage);

            // place the chunk into the cache and unmark it as loading
            m_mutex.lock();
            {
                m_chunkGrid[row][col] = newChunk;
                m_currentlyLoading.erase(gridCoord);
            }
            m_mutex.unlock();
        }
    }

    unordered_set<pair<int, int>, PairHasher<int>> m_currentlyLoading;
    vector<vector<Chunk *>> m_chunkGrid;
    queue<pair<int, int>> m_loadQueue;
    thread m_thread;
    mutex m_mutex;
};