#pragma once

#include <SFML/Graphics.hpp>
#include <utility>

/**
 * Convert decimal degrees to meters.
 *
 * @param x: input decimal degrees value
 * @returns equivalent value in meters
 */
double degreesToMeters(double x)
{
    return x * 110773;  // value based on average conversion for latitude of map area 
}

/**
 * Convert meters to decimal degrees.
 *
 * @param x: input meters value
 * @returns equivalent value in decimal degrees
 */
double metersToDegrees(double x)
{
    return x / 110773;  // value based on average conversion for latitude of map area
}

/**
 * Convert decimal degrees to pixels given the conversion ratio.
 *
 * @param x: input decimal degrees value
 * @param pixelsPerDegree: ratio of pixels per degree
 * @returns equivalent value in pixels
 */
double degreesToPixels(double x, double pixelsPerDegree)
{
    return x * pixelsPerDegree;
}

/**
 * Convert pixels to decimal degrees given the terms of the conversion ratio.
 *
 * @param x: input pixel value
 * @param degreesPerPixel: ratio of degrees per pixel
 * @returns equivalent value in decimal degrees
 */
double pixelsToDegrees(double x, double degreesPerPixel)
{
    return x * degreesPerPixel;
}

/**
 * Calculate the euclidean distance between two points.
 * 
 * @returns distance between the two points
*/
double distanceBetweenPoints(double x0, double y0, double x1, double y1) {
    return sqrt(pow(x1 - x0, 2) + pow(y1 - y0, 2));
}

/**
 * extension of sf::Rect with additional functionality.
 */
template <typename T>
struct Rectangle : public sf::Rect<T>
{
    Rectangle() {}
    Rectangle(T top, T left, T width, T height) : sf::Rect<T>(left, top, width, height) {}

    /**
     * Get the right edge of the rectangle.
     *
     * @returns right edge coordinate
     */
    T right() const
    {
        return this->left + this->width;
    }

    /**
     * Get the bottom edge of the rectangle.
     *
     * @returns bottom edge coordinate
     */
    T bottom() const
    {
        return this->top + this->height;
    }

    /**
     * Scale the rectangle by a given ratio.
     *
     * @param ratio: scaling ratio
     * @returns scaled Rectangle
     */
    Rectangle<T> scale(const T &ratio) const
    {
        return Rectangle<T>(this->top * ratio, this->left * ratio, this->width * ratio, this->height * ratio);
    }

    /**
     * Center the rectangle on a specified point.
     *
     * @param pointVector: center point coordinates
     */
    void centerOnPoint(sf::Vector2<T> pointVector)
    {
        this->left = pointVector.x - this->width / 2;
        this->top = pointVector.y - this->height / 2;
    }
};

/**
 * Class representing map geometry operations.
 */
class MapGeometry
{
public:
    MapGeometry() {}

    /**
     * Constructor for MapGeometry.
     *
     * @param pixelsPerDegree: ratio of pixels per degree
     * @param mapGeoBounds: geographical bounds of the map
     * @param chunkGeoSize: size of map chunks in decimal degrees
     */
    MapGeometry(double pixelsPerDegree, Rectangle<double> mapGeoBounds, double chunkGeoSize)
    : pixelsPerDegree(pixelsPerDegree), mapGeoBounds(mapGeoBounds), mapDisplayBounds(0,0,0,0),
    chunkGeoSize(chunkGeoSize)
    {
        mapDisplayBounds.width = degreesToPixels(mapGeoBounds.width, pixelsPerDegree);
        mapDisplayBounds.height = degreesToPixels(mapGeoBounds.height, pixelsPerDegree);
    }

    /**
     * Get the display bounds of the map in pixels.
     *
     * @returns Rectangle representing display bounds
     */
    const Rectangle<double> getDisplayBounds() const
    {
        return mapDisplayBounds;
    }

    /**
     * Get the row and column of a chunk
     *
     * @param offsetLatitude: offset latitude in degrees
     * @param offsetLongitude: offset longitude in degrees
     * @returns A std pair containing row = first and column = second
    */
    std::pair<int, int> getChunkRowCol(double offsetLatitude, double offsetLongitude) const {
        std::pair<int, int> chunkCoordinate;
        chunkCoordinate.first = int(offsetLatitude / getChunkGeoSize());
        chunkCoordinate.second = int(offsetLongitude / getChunkGeoSize());
        return chunkCoordinate;
    }

    /**
     * Get the size of map chunks in decimal degrees.
     *
     * @returns chunk size in decimal degrees
     */
    double getChunkGeoSize() const
    {
        return chunkGeoSize;
    }

    /**
     * Get the size of map chunks in display units (pixels).
     *
     * @returns chunk size in pixels
     */
    double getChunkDisplaySize() const
    {
        return chunkGeoSize * pixelsPerDegree;
    }

    /**
     * Convert a geographical vector to a pixel vector.
     *
     * @param geoVector: geographical vector
     * @returns corresponding pixel vector
     */
    sf::Vector2<double> toPixelVector(const sf::Vector2<double> &geoVector) const
    {
        auto res = sf::Vector2<double>(geoVector.x, geoVector.y);
        res.x *= pixelsPerDegree;
        res.y *= pixelsPerDegree;
        return res;
    }

    /**
     * Convert a pixel vector to a geographical vector.
     *
     * @param pixelVector: pixel vector
     * @returns corresponding geographical vector
     */
    sf::Vector2<double> toGeoVector(const sf::Vector2<double> &pixelVector) const
    {
        auto res = sf::Vector2<double>(pixelVector.x, pixelVector.y);
        res.x /= pixelsPerDegree;
        res.y /= pixelsPerDegree;
        return res;
    }

    /**
     * Convert a geographical rectangle to a pixel rectangle.
     *
     * @param geoRectangle: geographical rectangle
     * @returns corresponding pixel rectangle
     */
    Rectangle<double> toPixelRectangle(const Rectangle<double> &geoRectangle) const
    {
        auto res = Rectangle<double>(geoRectangle);
        res.left *= pixelsPerDegree;
        res.top *= pixelsPerDegree;
        res.width *= pixelsPerDegree;
        res.height *= pixelsPerDegree;
        return res;
    }

    /**
     * Convert a pixel rectangle to a geographical rectangle.
     *
     * @param pixelRectangle: pixel rectangle
     * @returns corresponding geographical rectangle
     */
    Rectangle<double> toGeoRectangle(const Rectangle<double> &pixelRectangle) const
    {
        auto res = Rectangle<double>(pixelRectangle);
        res.left /= pixelsPerDegree;
        res.top /= pixelsPerDegree;
        res.width /= pixelsPerDegree;
        res.height /= pixelsPerDegree;
        return res;
    }

    /**
     * Offset a geographical vector relative to the map bounds.
     *
     * @param geoVector: geographical vector to offset
     * @returns offset geographical vector
     */
    sf::Vector2<double> offsetGeoVector(const sf::Vector2<double> &geoVector) const
    {
        auto res = sf::Vector2<double>(geoVector);
        res.x = geoVector.x - mapGeoBounds.left;
        res.y = mapGeoBounds.top - geoVector.y;
        return res;
    }

    /**
     * Reverse offset a geographical vector relative to the map bounds.
     *
     * @param geoVector: geographical vector to reverse offset
     * @returns reverse offset geographical vector
     */
    sf::Vector2<double> unoffsetGeoVector(const sf::Vector2<double> &geoVector) const
    {
        auto res = sf::Vector2<double>(geoVector);
        res.x = geoVector.x + mapGeoBounds.left;
        res.y = mapGeoBounds.top - geoVector.y;
        return res;
    }

    /**
     * Get the maximum row index of map chunks.
     *
     * @returns maximum row index
     */
    int maxChunkRow() const
    {
        return int(mapGeoBounds.height / chunkGeoSize);
    }

    /**
     * Get the maximum column index of map chunks.
     *
     * @returns maximum column index
     */
    int maxChunkCol() const
    {
        return int(mapGeoBounds.width / chunkGeoSize);
    }

    /**
     * Calculate overlapping chunks for a given geographical rectangle.
     *
     * @param geoRectangle: geographical rectangle
     * @returns Rectangle representing overlapping chunks
     */
    Rectangle<int> calculateOverlappingChunks(const Rectangle<double> &geoRectangle) const
    {
        int topRow = int(geoRectangle.top / chunkGeoSize);
        int bottomRow = int(geoRectangle.bottom() / chunkGeoSize);
        int leftCol = int(geoRectangle.left / chunkGeoSize);
        int rightCol = int(geoRectangle.right() / chunkGeoSize);
        return Rectangle<int>(topRow, leftCol, rightCol - leftCol, bottomRow - topRow);
    }

    /**
     * Check if a chunk grid coordinate is valid.
     *
     * @param row: row index
     * @param col: column index
     * @returns true if the coordinate is valid, false otherwise
     */
    bool isValidChunkGridCoordinate(int row, int col)
    {
        return row >= 0 && row <= maxChunkRow() && col >= 0 && col <= maxChunkCol();
    }

private:
    double pixelsPerDegree;
    double chunkGeoSize;
    Rectangle<double> mapGeoBounds;
    Rectangle<double> mapDisplayBounds;
};
