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

    void start(float chunkGeoSize, string dbFilePath)
    {
        // this flag will be set to true to stop the workers
        m_stopWorkers = false;
        // start threads that load chunks from the db so that
        // chunks can be loaded in the background without freezing the app
        auto workerTask = [this, chunkGeoSize, dbFilePath]()
        {
            this->workerThread(chunkGeoSize, dbFilePath);
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
            m_cache[row].resize(col + 1, nullptr);  // cache slots are init'd as nullptr
            m_isLoading[row].resize(col + 1, false);  // flag grid slots are init'd false
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
                m_loadQueue.push({ row, col });
                m_isLoading[row][col] = true;
            }
        }
        m_mutex.unlock();
    }

    void workerThread(float chunkGeoSize, string dbFilePath)
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
            Chunk *newChunk = new Chunk(data, chunkGeoSize, &storage);

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