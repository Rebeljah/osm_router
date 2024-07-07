#pragma once

#include <SFML/Graphics.hpp>

double degreesToMeters(double x)
{
    return x * 110773;  // value based on average conversion for latitude of map area 
}

double metersToDegrees(double x)
{
    return x / 110773;  // value based on average conversion for latitude of map area
}

/*
Convert decimal degrees to pixels given the conversion ratio.

let p and d such that p px = d deg. Therefore, 1 deg = (p/d) px, and
x deg = x * (p/d) px.

@param x: input decimal degrees value
@param pixelsPerDegree: ratio of pixels per degree
*/
double degreesToPixels(double x, double pixelsPerDegree)
{
    return x * pixelsPerDegree;
}

/*
Convert pixels to decimal degrees given the terms of the conversion ratio.

let d and p such that p px = d deg. Therefore, 1 px = (d/p) deg and
x px = x * (d/p) deg

@param x: input pixel value
@param degreesPerPixel: ratio of degrees per pixel
*/
double pixelsToDegrees(double x, double degreesPerPixel)
{
    return x * degreesPerPixel;
}

template <typename T>
struct Rectangle : public sf::Rect<T>
{
    Rectangle() {}
    Rectangle(T top, T left, T width, T height) : sf::Rect<T>(left, top, width, height) {}

    T right() const
    {
        return this->left + this->width;
    }

    T bottom() const
    {
        return this->top + this->height;
    }

    Rectangle<T> scale(const T &ratio) const
    {
        return Rectangle<T>(this->top * ratio, this->left * ratio, this->width * ratio, this->height * ratio);
    }

    void centerOnPoint(sf::Vector2<T> pointVector)
    {
        this->left = pointVector.x - this->width / 2;
        this->top = pointVector.y - this->height / 2;
    }
};

class MapGeometry
{
public:
    MapGeometry() {}

    MapGeometry(double pixelsPerDegree, Rectangle<double> mapGeoBounds, double chunkGeoSize)
    : pixelsPerDegree(pixelsPerDegree), mapGeoBounds(mapGeoBounds), mapDisplayBounds(0,0,0,0),
    chunkGeoSize(chunkGeoSize)
    {
        mapDisplayBounds.width = degreesToPixels(mapGeoBounds.width, pixelsPerDegree);
        mapDisplayBounds.height = degreesToPixels(mapGeoBounds.height, pixelsPerDegree);
    }

    const Rectangle<double> getDisplayBounds() const
    {
        return mapDisplayBounds;
    }

    double getChunkGeoSize() const
    {
        return chunkGeoSize;
    }

    double getChunkDisplaySize() const
    {
        return chunkGeoSize * pixelsPerDegree;
    }

    sf::Vector2<double> toPixelVector(const sf::Vector2<double> &geoVector) const
    {
        auto res = sf::Vector2<double>(geoVector.x, geoVector.y);
        res.x *= pixelsPerDegree;
        res.y *= pixelsPerDegree;
        return res;
    }

    sf::Vector2<double> toGeoVector(const sf::Vector2<double> &pixelVector) const
    {
        auto res = sf::Vector2<double>(pixelVector.x, pixelVector.y);
        res.x /= pixelsPerDegree;
        res.y /= pixelsPerDegree;
        return res;
    }

    Rectangle<double> toPixelRectangle(const Rectangle<double> &geoRectangle)
    {
        auto res = Rectangle<double>(geoRectangle);
        res.left *= pixelsPerDegree;
        res.top *= pixelsPerDegree;
        res.width *= pixelsPerDegree;
        res.height *= pixelsPerDegree;
        return res;
    }

    Rectangle<double> toGeoRectangle(const Rectangle<double> &pixelRectangle)
    {
        auto res = Rectangle<double>(pixelRectangle);
        res.left /= pixelsPerDegree;
        res.top /= pixelsPerDegree;
        res.width /= pixelsPerDegree;
        res.height /= pixelsPerDegree;
        return res;
    }

    sf::Vector2<double> offsetGeoVector(const sf::Vector2<double> &geoVector)
    {
        auto res = sf::Vector2<double>(geoVector);
        res.x = geoVector.x - mapGeoBounds.left;
        res.y = mapGeoBounds.top - geoVector.y;
        return res;
    }

    sf::Vector2<double> unoffsetGeoVector(const sf::Vector2<double> &geoVector)
    {
        auto res = sf::Vector2<double>(geoVector);
        res.x = geoVector.x + mapGeoBounds.left;
        res.y = geoVector.y + mapGeoBounds.bottom();
        return res;
    }

    Rectangle<int> calculateOverlappingChunks(const Rectangle<double> &geoRectangle) const
    {
        int topRow = int(geoRectangle.top / chunkGeoSize);
        int bottomRow = int(geoRectangle.bottom() / chunkGeoSize);
        int leftCol = int(geoRectangle.left / chunkGeoSize);
        int rightCol = int(geoRectangle.right() / chunkGeoSize);
        return Rectangle<int>(topRow, leftCol, rightCol - leftCol, bottomRow - topRow);
    }

private:
    double pixelsPerDegree;
    double chunkGeoSize;
    Rectangle<double> mapGeoBounds;
    Rectangle<double> mapDisplayBounds;
};