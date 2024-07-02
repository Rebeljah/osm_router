#pragma once

#include <SFML/Graphics.hpp>

typedef double Degrees;
typedef double Pixels;
typedef double Meters;

Meters degreesToMeters(Degrees x)
{
    return x * 110773;
}

Degrees metersToDegrees(Meters x)
{
    return x / 110773;
}

/*
Convert decimal degrees to pixels given the conversion ratio.

let p and d such that p px = d deg. Therefore, 1 deg = (p/d) px, and
x deg = x * (p/d) px.

@param x: input decimal degrees value
@param pd: ratio of pixels per degree
*/
Pixels degreesToPixels(Degrees x, double pd)
{
    return x * pd;
}

/*
Convert pixels to decimal degrees given the terms of the conversion ratio.

let d and p such that p px = d deg. Therefore, 1 px = (d/p) deg and
x px = x * (d/p) deg

@param x: input pixel value
@param dp: ratio of degrees per pixel
*/
Degrees pixelsToDegrees(Pixels x, double dp)
{
    return x * dp;
}

struct DisplayRect : public sf::Rect<Pixels>
{
    DisplayRect(Pixels top, Pixels left, Pixels w, Pixels h) : sf::Rect<Pixels>(left, top, w, h) {}
    DisplayRect() : sf::Rect<Pixels>(0, 0, 0, 0) {}
    Pixels right() const
    {
        return left + width;
    }
    Pixels bottom() const
    {
        return top + height;
    }
};

struct GeoRect : public sf::Rect<Degrees>
{
    GeoRect(Degrees top, Degrees left, Degrees w, Degrees h) : sf::Rect<Degrees>(left, top, w, h) {}
    GeoRect() : sf::Rect<Degrees>(0, 0, 0, 0) {}
    Degrees right() const
    {
        return left + width;
    }
    Degrees bottom() const
    {
        return top + height;
    }
};

/*
Convert a geo rect in decimal degrees to a pixel rect given a conversion ratio.
@param r: input rect
@param pd: ratio of pixels per degree
*/
DisplayRect geoRectToDisplayRect(GeoRect r, double pd)
{
    auto f = [pd](Pixels x)
    { return degreesToPixels(x, pd); };
    return DisplayRect(f(r.top), f(r.left), f(r.width), f(r.height));
}

/*
Convert a display rect in pixels to a georect in decimal degrees given a conversion ratio.
@param r: input rect
@param dp: ratio of degrees per pixel
*/
GeoRect displayRectToGeoRect(DisplayRect r, double dp)
{
    auto f = [dp](Pixels x)
    { return pixelsToDegrees(x, dp); };
    return GeoRect(f(r.top), f(r.left), f(r.width), f(r.height));
}