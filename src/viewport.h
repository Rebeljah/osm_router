#pragma once

#include "geometry.h"

enum class PanDirection
{
    Up,
    Down,
    Left,
    Right,
};

class Viewport : public Rectangle<float>
{
public:
    Viewport() {}

    Viewport(float w, float h, float mapWidth, float mapHeight) : Rectangle<float>(0, 0, w, h)
    {
        // this rect defines the area that the viewport is bound to
        // the user will not be able to move the viewport out of this area
        this->boundingArea = Rectangle<float>(0, 0, mapWidth, mapHeight);
    }

    Viewport &operator=(const Viewport &other)
    {
        this->top = other.top;
        this->left = other.left;
        this->width = other.width;
        this->height = other.height;
        this->boundingArea = other.boundingArea;
        return *this;
    }

    /*
    This function can be used to start or stop the viewport moving in a certain
    direction.
    @param direction: One of Up,Down,Left,Right
    @param isPanning: true or false to turn on or off panning in the given direction
    */
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

    /*
    This should be called on each frame to move the viewport around.
    @param deltaTime: Time elapsed since the last frame, ensures smooth movement
     with varying framerates.
    */
    void update(float deltaTime)
    {
        float nPixels = panVelocity * deltaTime;

        if (isPanningUp ^ isPanningDown) // one or the other but not both
        {
            if (isPanningUp)
            {
                top -= nPixels; // move up
            }

            if (isPanningDown)
            {
                top += nPixels; // move down
            }
        }

        if (isPanningLeft ^ isPanningRight) // stops from trying to pan left and right at same time
        {
            if (isPanningLeft)
            {
                left -= nPixels; // move left
            }

            if (isPanningRight)
            {
                left += nPixels; // move right
            }
        }

        // check collision with pannable area boundaries
        if (left < boundingArea.left)
        {
            left = boundingArea.left;
        }
        else if (right() > boundingArea.right())
        {
            left = boundingArea.right() - width;
        }
        if (top < boundingArea.top)
        {
            top = boundingArea.top;
        }
        else if (bottom() > boundingArea.bottom())
        {
            top = boundingArea.bottom() - height;
        }
    }

    /*
    Convert a point that is relative to the viewport to one that is
    relative to the viewport's bounding area. For example, if the viewport
    is at the top-left of the bounding area, then this function has no effect. If 
    the viewport's top-left is 10 pixels from the left of the bounding area, and 10 pixels
    from the top of the bounding area this function would return { 10, 10 } given
    { 0, 0 }
    @param xy: vector of the point relative to the viewport's topleft
    @returns an xy vector relative to the topleft origin of the bounding area
    */
    sf::Vector2f viewportPostoMapPos(sf::Vector2f xy)
    {
        return { xy.x + left, xy.y + top };
    }

private:
    Rectangle<float> boundingArea;

    // panning controls
    int panVelocity = 450; // pixels per second
    bool isPanningLeft = false;
    bool isPanningRight = false;
    bool isPanningUp = false;
    bool isPanningDown = false;
};