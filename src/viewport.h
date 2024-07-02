#pragma once

#include "geometry.h"

enum class PanDirection
{
    Up,
    Down,
    Left,
    Right,
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