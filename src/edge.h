#pragma once

#include <vector>
#include <string>

#include <SFML/Graphics.hpp>

#include "sql.h"
#include "utils.h"
#include "geometry.h"

using std::string;
using std::vector;

// path_descriptor_to_int = {"Forbidden": 0, "Allowed": 1, "Residential": 2, "Tertiary": 3, "Secondary": 4, "Primary": 5, "Trunk": 6, "Motorway": 7, "Track": 8, "Lane": 8}
enum class PathDescriptor
{
    Forbidden,
    Allowed,
    Residential,
    Tertiary,
    Secondary,
    Primary,
    Trunk,
    Motorway,
    Track,
    Lane
};

struct PointPath
{
    vector<sf::Vector2<double>> points;

    PointPath() {}
    PointPath(string wktLinestring)
    {
        // string "4.3 1.2, 4.4 1.0," -> vector { {4.3, 1.2}, {4.4, 1.0} }
        for (auto part : splitString(wktLinestring, ","))
        {
            auto lonLat = splitString(part, " ");
            auto lon = stod(lonLat.at(0));
            auto lat = stod(lonLat.at(1));
            points.push_back(sf::Vector2<double>(lon, lat));
        }
    }

    void extend(const PointPath &other)
    {
        points.insert(points.end(), other.points.begin(), other.points.end());
    }

    void reverse()
    {
        std::reverse(points.begin(), points.end());
    }

    void clear()
    {
        points.clear();
    }
    /*
    Get a lat / lon rectangle that bounds the path. The left and right will be the min and max
    longitude respectively, and the top and bottom will be, respectively, the min and max
    latitude. (min latitude is at the top because the lat,lon is stored as offset
    from the top left of the map area)
    @returns The smallest rectangle that bounds the entire path.
    */
    Rectangle<double> getGeoBoundingBox()
    {
        double minLon = 1000000;
        double maxLon = -1000000;
        double minLat = 1000000;
        double maxLat = -1000000;

        for (const auto &[lon, lat] : points)
        {
            if (lon < minLon)
                minLon = lon;
            if (lon > maxLon)
                maxLon = lon;
            if (lat < minLat)
                minLat = lat;
            if (lat > maxLat)
                maxLat = lat;
        }

        return Rectangle<double>(minLat, minLon, maxLon - minLon, maxLat - minLat);
    }
};

struct Edge
{
    sql::Edge data;
    PointPath path;
    sf::Color color;

    Edge(sql::Edge data) : data(data), path(data.pathOffsetPoints)
    {
        // set the color of the edge based on the type of path
        // for example: highways can be colored red, while bike/hiking only paths can
        // be green
        color = sf::Color(95, 188, 89, 255);

        auto carFwd = (PathDescriptor)data.pathCarFwd;
        auto carBwd = (PathDescriptor)data.pathCarBwd;

        if (
            carFwd == PathDescriptor::Motorway || carFwd == PathDescriptor::Trunk ||
            carBwd == PathDescriptor::Motorway || carBwd == PathDescriptor::Trunk)
        {
            // color = sf::Color(112, 144, 178, 255);
            color = sf::Color(70, 130, 180, 255); // Blue
        }
        else if (
            carFwd == PathDescriptor::Primary || carFwd == PathDescriptor::Secondary ||
            carBwd == PathDescriptor::Primary || carBwd == PathDescriptor::Secondary)
        {
            // color = sf::Color(0, 0, 0, 255);
            color = sf::Color(255, 165, 0, 255); // Orange
        }
        else if (
            carFwd == PathDescriptor::Tertiary || carFwd == PathDescriptor::Residential ||
            carBwd == PathDescriptor::Tertiary || carBwd == PathDescriptor::Residential)
        {
            color = sf::Color(198, 202, 210, 255); // Gray
        }
    }
};

struct Route
{
    Route() {}
    Route(PointPath path, MapGeometry *mapGeometry) : path(path), mapGeometry(mapGeometry) {}

    PointPath path;
    MapGeometry *mapGeometry;

    void render(sf::RenderWindow &window, Rectangle<double> viewportRect)
    {
        sf::VertexArray pathVertices(sf::LineStrip, path.points.size());
        for (int i = 0; i < path.points.size(); ++i)
        {
            // convert the offset lon, lat to a map-relative pixel coordinate
            auto pointDisplayCoordinate = mapGeometry->toPixelVector(path.points[i]);
            // offset the coordinate to the viewport display rectangle
            pointDisplayCoordinate -= {viewportRect.left, viewportRect.top};

            pathVertices[i].color = sf::Color::Blue;
            pathVertices[i].position = sf::Vector2f(pointDisplayCoordinate);
        }
        // draw the completed path
        window.draw(pathVertices);
    }
};