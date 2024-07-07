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

    void centerOnPoint(T x, T y)
    {
        this->left = x - this->width / 2;
        this->top = y - this->height / 2;
    }
};