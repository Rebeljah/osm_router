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

using std::unordered_map;
using std::vector;
using std::unordered_set;
using std::queue;
using std::mutex;
using std::pair;
using std::thread;
using std::string;

typedef string ChunkID;

ChunkID chunkId(int row, int col)
{
    return std::to_string(row) + ',' + std::to_string(col);
}

struct Chunk
{
    sql::Chunk data;
    GeoRect geoBbox;
    unordered_map<NodeID, Node> nodes;

    Chunk(sql::Chunk chunk, Degrees size, sql::Storage *storage)
    {
        this->data = chunk;
        this->geoBbox = GeoRect(chunk.offsetLatTop, chunk.offsetLonLeft, size, size);

        // prealloc space for all nodes
        nodes.reserve(chunk.numNodes);

        using namespace sqlite_orm;

        // load all nodes and attach out-edges to them

        for (auto sqlNode : storage->iterate<sql::Node>(where(c(&sql::Node::chunkId) == chunk.id), limit(chunk.numNodes)))
        {
            nodes.emplace(sqlNode.id, Node(sqlNode));
        }

        for (auto sqlEdge : storage->iterate<sql::Edge>(where(c(&sql::Edge::chunkId) == chunk.id), limit(chunk.numEdges)))
        {
            nodes.at(sqlEdge.sourceNodeId).edgesOut.push_back(Edge(sqlEdge));
        }
    }
};

class ChunkLoader
{
public:
    void start(sql::Storage *storage, Degrees chunkSize)
    {
        t1 = std::thread([this, storage, chunkSize](){ this->loaderThread(storage, chunkSize); });
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

    void loaderThread(sql::Storage *storage, Degrees chunkSize)
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
                    c(&sql::Chunk::row) == row and
                    c(&sql::Chunk::col) == col
                ),
                limit(1)
            ).at(0);

            Chunk *newChunk = new Chunk(sqlChunk, chunkSize, storage);

            m_mutex.lock();

            m_chunkGrid[row][col] = newChunk;
            m_currentlyLoading.erase(gridCoord);

            m_mutex.unlock();
        }
    }

    vector<vector<Chunk *>> m_chunkGrid;
    mutex m_mutex;
    queue<pair<int, int>> m_loadQueue;
    unordered_set<pair<int, int>, PairHasher<int>> m_currentlyLoading;
    thread t1;
};