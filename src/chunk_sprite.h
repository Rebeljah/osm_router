#pragma once

#include <unordered_map>
#include <queue>

#include <SFML/Graphics.hpp>

#include "chunk.h"

using std::unordered_map;
using std::vector;
using std::queue;

struct ChunkSprite : sf::Sprite
{
    ChunkSprite(DisplayRect rect) : rect(rect)
    {  
        renderTexture.create(rect.width+1, rect.height+1);
        this->setTexture(renderTexture.getTexture());
    }

    void renderEdge(Edge &edge, double pd)
    {
        sf::VertexArray path(sf::LineStrip, edge.path.points.size());
        for (int i = 0; i < edge.path.points.size(); ++i)
        {
            Degrees offsetLat = edge.path.points[i].y;
            Degrees offsetLon = edge.path.points[i].x;
            Pixels x = degreesToPixels(offsetLon, pd) - rect.left;
            Pixels y = degreesToPixels(offsetLat, pd) - rect.top;

            path[i].color = edge.color;
            path[i].position = {x, y};
        }
        renderTexture.draw(path);
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

        if (m_grid.size() <= row)
            m_grid.resize(row + 1);

        if (m_grid[row].size() <= col)
            m_grid[row].resize(col + 1);
        
        // return the sprite if already loaded
        if (m_grid[row][col] != nullptr)
        {
            return m_grid[row][col];
        }

        // return null option if the chunk is not loaded yet
        std::optional<Chunk *> chunkOpt;
        if (!(chunkOpt = m_pChunkLoader->get(row, col)).has_value())
        {
            return std::nullopt;
        }

        // chunk is loaded, so it can be used to render sprite
        renderChunkSprite(**chunkOpt, row, col);
        renderInterchunkEdges();
        
        m_grid[row][col]->renderTexture.display();
        return m_grid[row][col];
    }

private:
    void renderChunkSprite(Chunk &chunk, int row, int col)
    {
        DisplayRect chunkRect = DisplayRect(
            degreesToPixels(chunk.data.offsetLatTop, m_pd),
            degreesToPixels(chunk.data.offsetLonLeft, m_pd),
            m_chunkSize,
            m_chunkSize
        );

        auto chunkSprite = new ChunkSprite(chunkRect);
        
        for (auto &[_, node] : chunk.nodes)
        {
            for (auto &edge : node.edgesOut)
            {
                chunkSprite->renderEdge(edge, m_pd);

                // find edges that cross into other chunks
                GeoRect bbox = edge.path.getBoundingBox();
                int r0 = int(bbox.top / pixelsToDegrees(m_chunkSize, 1/m_pd));
                int r1 = int(bbox.bottom() / pixelsToDegrees(m_chunkSize, 1/m_pd));
                int c0 = int(bbox.left / pixelsToDegrees(m_chunkSize, 1/m_pd));
                int c1 = int(bbox.right() / pixelsToDegrees(m_chunkSize, 1/m_pd));
                for (int r = r0; r <= r1; ++r)
                {
                    for (int c = c0; c <= c1; ++c)
                    {
                        if (r == chunk.data.row && c == chunk.data.col)
                            continue;
                        
                        interChunkEdges.push(std::make_pair(std::make_pair(r, c), &edge));
                    }
                }
            }
        }

        m_grid[row][col] = chunkSprite;
    }

    void renderInterchunkEdges()
    {
        int n_in_queue = interChunkEdges.size();
        for (int i = 0; i < n_in_queue; ++i)
        {
            auto item = interChunkEdges.front();
            auto [coord, pEdge] = item;
            auto [row, col] = coord;
            interChunkEdges.pop();

            if (m_grid.size() <= row)
                m_grid.resize(row + 1);
            if (m_grid[row].size() <= col)
                m_grid[row].resize(col + 1);

            ChunkSprite *chunkSprite = m_grid[row][col];

            // chunk sprite not yet loaded, save item for later
            if (chunkSprite == nullptr)
            {
                interChunkEdges.push(item);
                continue;
            }

            chunkSprite->renderEdge(*pEdge, m_pd);
        }
    }

    vector<vector<ChunkSprite *>> m_grid;
    ChunkLoader *m_pChunkLoader;
    Pixels m_chunkSize;
    double m_pd;
    queue<pair<pair<int, int>, Edge *>> interChunkEdges;
};