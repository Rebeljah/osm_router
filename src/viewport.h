#pragma once

#include "geometry.h"

enum class PanDirection
{
    Up,
    Down,
    Left,
    Right,
};

class Viewport : public Rectangle<double>
{
public:
    Viewport() {}

    Viewport(sf::Vector2<double> displaySize, MapGeometry *mapGeometry)
        : Rectangle<double>(0, 0, displaySize.x, displaySize.y), mapGeometry(mapGeometry)
    {
    }

    /*
    This function can be used to start or stop the viewport moving in a certain
    direction.
    @param keyEvent: should be a keydown or keyup event
    */
    void controlPanning(sf::Event keyEvent)
    {
        bool isPressed = keyEvent.type == sf::Event::KeyPressed;

        switch (keyEvent.key.code)
        {
        case sf::Keyboard::Up:
            isPanningUp = isPressed;
            break;
        case sf::Keyboard::Down:
            isPanningDown = isPressed;
            break;
        case sf::Keyboard::Left:
            isPanningLeft = isPressed;
            break;
        case sf::Keyboard::Right:
            isPanningRight = isPressed;
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

        auto boundingArea = mapGeometry->getDisplayBounds();
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
    sf::Vector2f windowPositionToMapPosition(sf::Vector2f xy)
    {
        return {xy.x + left, xy.y + top};
    }

private:
    MapGeometry *mapGeometry;

    // panning controls
    int panVelocity = 450; // pixels per second
    bool isPanningLeft = false;
    bool isPanningRight = false;
    bool isPanningUp = false;
    bool isPanningDown = false;
};