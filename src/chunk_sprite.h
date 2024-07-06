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
    ChunkSprite(Rectangle<float> rect) : rect(rect)
    {  
        // Drawing will be done on a RenderTexture so that it can be cached
        // this avoid redrawing all of the roads on each frame
        renderTexture.create(rect.width+1, rect.height+1);
        this->setTexture(renderTexture.getTexture());
    }

    void renderEdge(Edge &edge, double pixelsPerDegree)
    {
        // the edges path is rendered by loading it's points into a vertex array
        // then drawing the vertex array to the RenderTexture.
        // the edge path points are loaded as offset lon, lat coordinates,
        // (if a point is at the topleft of the map area, then it's coordinates will be 0,0).
        sf::VertexArray path(sf::LineStrip, edge.path.points.size());
        for (int i = 0; i < edge.path.points.size(); ++i)
        {
            double lon = edge.path.points[i].x;
            double lat = edge.path.points[i].y;
            // convert the offset lon, lat to pixels and then offset to the
            // chunk's rectangle.
            auto x = (float)degreesToPixels(lon, pixelsPerDegree) - rect.left;
            auto y = (float)degreesToPixels(lat, pixelsPerDegree) - rect.top;

            path[i].color = edge.color;
            path[i].position = {x, y};
        }
        renderTexture.draw(path);
    }

    sf::RenderTexture renderTexture;
    Rectangle<float> rect;
};

class ChunkSpriteLoader
{
public:
    ChunkSpriteLoader() {}

    ChunkSpriteLoader(ChunkLoader *pChunkLoader, float chunkGeoSize, double pixelsPerDegree)
    : m_pChunkLoader(pChunkLoader),
      m_chunkGeosize(chunkGeoSize),
      m_pixelsPerDegree(pixelsPerDegree)
    {
    }

    void init(ChunkLoader *pChunkLoader, float chunkGeoSize, double pixelsPerDegree)
    {
        m_pChunkLoader = pChunkLoader;
        m_chunkGeosize = chunkGeoSize;
        m_pixelsPerDegree = pixelsPerDegree;
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
        // convert the offset lan, lon stored in the db to a pixel position
        auto rect = Rectangle<float>(
            (float)degreesToPixels(chunk.data.offsetLatTop, m_pixelsPerDegree),
            (float)degreesToPixels(chunk.data.offsetLonLeft, m_pixelsPerDegree),
            m_chunkGeosize,
            m_chunkGeosize
        );

        auto chunkSprite = new ChunkSprite(rect);
        
        // render all edges in the chunk onto the chunkSprite texture
        for (auto &[_, node] : chunk.nodes)
        {
            for (auto &edge : node.edgesOut)
            {
                chunkSprite->renderEdge(edge, m_pixelsPerDegree);

                // find edges that cross into other chunks
                // This work by getting a bounding box that encompasses the edge,
                // if the bounding box overlaps other chunks, these will be added
                // to the map and drawn onto the other chunk/s on the next frame.
                Rectangle<float> bbox = edge.path.getGeoBoundingBox();
                // get the range of chunks coordinates (r0 to r1, c0 to c1) that the
                // edge's bounding box intersects
                int r0 = int(bbox.top / pixelsToDegrees(m_chunkGeosize, 1/m_pixelsPerDegree));
                int r1 = int(bbox.bottom() / pixelsToDegrees(m_chunkGeosize, 1/m_pixelsPerDegree));
                int c0 = int(bbox.left / pixelsToDegrees(m_chunkGeosize, 1/m_pixelsPerDegree));
                int c1 = int(bbox.right() / pixelsToDegrees(m_chunkGeosize, 1/m_pixelsPerDegree));
                for (int r = r0; r <= r1; ++r)
                {
                    for (int c = c0; c <= c1; ++c)
                    {
                        if (r == chunk.data.row && c == chunk.data.col)
                            continue;  // don't redner edge again on current chunk
                        
                        interChunkEdges.push(std::make_pair(std::make_pair(r, c), &edge));
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
            auto [coord, pEdge] = item;
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
            m_grid[row][col]->renderEdge(*pEdge, m_pixelsPerDegree);
        }
    }

    queue<pair<pair<int, int>, Edge *>> interChunkEdges;
    vector<vector<ChunkSprite *>> m_grid;
    ChunkLoader *m_pChunkLoader;
    float m_chunkGeosize;
    double m_pixelsPerDegree;
};