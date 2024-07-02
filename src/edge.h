#pragma once

#include <vector>
#include <string>

#include <SFML/Graphics.hpp>

#include "sql.h"
#include "utils.h"
#include "geometry.h"

using std::vector;
using std::string;

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

    PointPath(string wktLinestring)
    {
        for (auto part : splitString(wktLinestring, ","))
        {
            auto lonLat = splitString(part, " ");
            auto lon = stod(lonLat.at(0));
            auto lat = stod(lonLat.at(1));
            points.push_back(sf::Vector2<double>(lon, lat));
        }
    }

    PointPath() {}

    GeoRect getBoundingBox()
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

        return GeoRect(minLat, minLon, maxLon - minLon, maxLat - minLat);
    }
};

struct Edge
{
    sql::Edge data;
    PointPath path;
    sf::Color color;

    Edge(sql::Edge data) : data(data), path(data.pathOffsetPoints)
    {
        color = sf::Color::Magenta;

        if ((PathDescriptor)data.pathFoot != PathDescriptor::Forbidden || (PathDescriptor)data.pathBikeFwd != PathDescriptor::Forbidden || (PathDescriptor)data.pathBikeBwd != PathDescriptor::Forbidden)
        {
            color = sf::Color::Green;
        }

        if ((PathDescriptor)data.pathCarFwd != PathDescriptor::Forbidden || (PathDescriptor)data.pathCarBwd != PathDescriptor::Forbidden)
        {
            color = color = sf::Color::Blue;
        }

        if ((PathDescriptor)data.pathCarFwd == PathDescriptor::Motorway || (PathDescriptor)data.pathCarBwd == PathDescriptor::Motorway)
        {
            color = sf::Color::Red;
        }
    }
};