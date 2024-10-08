#pragma once

#include <thread>
#include <chrono>
#include <iostream>

#include "tomlplusplus/toml.hpp"

#include "geometry.h"
#include "viewport.h"
#include "chunk_sprite.h"
#include "nav_box.h"
#include "toasts.h"
#include "pubsub.h"

#include "graph.h"
#include "algorithms.h"
#include <utility>

class App
{
public:
    App() : window(sf::VideoMode(1080, 1080), "GatorMaps")
    {
        using Degree = double;
        Degree mapTop = *config["map"]["bbox_top"].value<double>();
        Degree mapLeft = *config["map"]["bbox_left"].value<double>();
        Degree mapBottom = *config["map"]["bbox_bottom"].value<double>();
        Degree mapRight = *config["map"]["bbox_right"].value<double>();
        Degree viewportW = *config["viewport"]["default_w"].value<double>();
        Degree chunkSize = *config["map"]["chunk_size"].value<double>();

        // connect the custom event queue to listen to events from different publishers
        eventQueue.subscribe(&navBox, ps::EventType::NavBoxSubmitted);
        eventQueue.subscribe(&navBox, ps::EventType::NavBoxFormChanged);
        eventQueue.subscribe(&algorithms, ps::EventType::NodeTouched);

        // load map data in background. Done event will be handled in event loop.
        std::thread([this]()
                    {
            this->mapGraph.load("./db/map.db");
            this->eventQueue.pushEvent(ps::Event(ps::EventType::MapDataLoaded)); })
            .detach();

        mapGeometry = MapGeometry(
            float(this->window.getSize().x) / viewportW,               // pixels per degree
            {mapTop, mapLeft, mapRight - mapLeft, mapTop - mapBottom}, // map geo area
            chunkSize                                                  // chunk geo size
        );

        route.mapGeometry = &mapGeometry;

        sf::Vector2<double> viewportGeoSize = {
            viewportW,
            viewportW * (float(this->window.getSize().y) / this->window.getSize().x)};

        viewport = Viewport(mapGeometry.toPixelVector(viewportGeoSize), &mapGeometry);

        viewport.centerOnPoint(
            mapGeometry.toPixelVector(
                mapGeometry.offsetGeoVector({-82.325005, 29.651982}) // Gville, FL
                ));

        chunkSpriteLoader.init(&mapGeometry, "./db/map.db");

        window.setFramerateLimit(*config["graphics"]["framerate"].value<int>());

        navBox.init(&window, &viewport, &mapGeometry, 250, 120);

        clock.restart();
    }

    ~App()
    {
    }

    void run()
    {
        toaster.spawnToast(window.getSize().x / 2, "Welcome to Navigator!", "1", sf::seconds(4.5));

        while (window.isOpen())
        {
            processEvents();
            update();
            render();
        }
    }

private:
    void processEvents()
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
                window.close();

            // Pan the map around by holding arrow keys
            if (event.type == sf::Event::KeyPressed || event.type == sf::Event::KeyReleased)
            {
                viewport.controlPanning(event);
                navBox.handleKeyPress(event);
            }

            // Handles clicks on the navbox elements
            if (event.type == sf::Event::MouseButtonPressed)
            {
                int x = event.mouseButton.x;
                int y = event.mouseButton.y;

                if (navBox.getBox().getGlobalBounds().contains(x, y))
                    navBox.handleClick(event);
                else
                {
                    navBox.updateCoordinates(event);
                }
            }
        }

        /*
        Listen for custom events that were pushed to the ps::EventQueue. List of
        event types and corresponding data types to access are in the ps namespace
        in pubsub.h. Depending on the event type, different variants of the event.data
        will be active. To extract the data struct, use std::get<DataType>(event.data).
        Refer to the comments in the ps::EventType enum to determine which `DataType` to access.
        */

        int nEvents = eventQueue.size();
        for (int i = 0; i < nEvents; ++i)
        {
            ps::Event event = eventQueue.popNext();

            if (event.type == ps::EventType::MapDataLoaded)
            {
                toaster.spawnToast(window.getSize().x / 2, "Map data loaded! Let's go!", "loading_data", sf::seconds(3));
            }
            else if (event.type == ps::EventType::NavBoxSubmitted)
            {
                startFindingRoute(event);
            }
            else if (event.type == ps::EventType::NodeTouched)
            {
                enqueueAnimationPoint(event);
            }
            else if (event.type == ps::EventType::NavBoxFormChanged)
            {
                clearAnimationPoints(event);
            }
            else if (event.type == ps::EventType::RouteCompleted)
            {
                onRouteCompleted(event);
            }
        }
    }

    void update()
    {
        // deltatime is the time elapsed since the last update
        // it is needed to smoothly update movement independent of framerate
        float deltaTime = clock.restart().asSeconds();
        viewport.update(deltaTime);
        toaster.update(deltaTime);
    }

    void render()
    {
        // window.clear(sf::Color(247, 246, 246, 255));
        window.clear(sf::Color(245, 245, 245, 255));

        // render up to x animation dots onto loaded chunk sprite
        // if a chunk sprite is not yet loaded, requue the point so that
        // it can be rendered later when the user pans that map region
        // into view
        int n = animationPoints.size();
        for (int i = 0; i < n && i < 1500; ++i)
        {
            auto [chunkCoord, offsetGeoCoord] = animationPoints.front();
            animationPoints.pop();

            auto [chunkRow, chunkCol] = chunkCoord;
            if (!chunkSpriteLoader.has(chunkRow, chunkCol))
            {
                animationPoints.push({chunkCoord, offsetGeoCoord});
                continue;
            }
            ChunkSprite *sprite = *chunkSpriteLoader.get(chunkRow, chunkCol);
            sprite->renderDot(offsetGeoCoord, &mapGeometry);
        }

        // determine the range of chunks that are inside of the viewport to render
        auto overlap = mapGeometry.calculateOverlappingChunks(mapGeometry.toGeoRectangle(viewport));

        for (int row = overlap.top - 1; row <= overlap.bottom() + 1; ++row)
        {
            for (int col = overlap.left - 1; col <= overlap.right() + 1; ++col)
            {
                // sometimes the buffer zone will overflow the map boundaries
                // we don't want to try loading those chunks
                if (!mapGeometry.isValidChunkGridCoordinate(row, col))
                    continue;

                // retrieve the chunk sprite if it is already rendered
                // if sprite is not rendered, the option will not have a value, so
                // the loop continues to the next chunk
                auto spriteOpt = chunkSpriteLoader.get(row, col);
                if (!spriteOpt.has_value())
                    continue;

                ChunkSprite &sprite = **spriteOpt;

                // skip drawing chunks that are buffered but not in the viewport
                if (row < overlap.top || row > overlap.bottom() || col < overlap.left || col > overlap.right())
                    continue;

                // draw chunk
                sprite.setPosition(sprite.rect.left - viewport.left, sprite.rect.top - viewport.top);
                window.draw(sprite);
            }
        }

        route.render(window, (Rectangle<double>)viewport);
        navBox.draw(window);
        toaster.render(window);
        window.display();
    }

    void startFindingRoute(ps::Event event)
    {
        // The graph is still loading, so notify the user
        if (!mapGraph.isDataLoaded())
        {
            toaster.spawnToast(window.getSize().x / 2, "Loading data, please wait...", "loading_data", sf::seconds(2.25));
            return;
        }

        auto navBoxForm = std::get<ps::Data::NavBoxForm>(event.data);
        sf::Vector2<double> origin = navBoxForm.origin;
        sf::Vector2<double> destination = navBoxForm.destination;
        AlgoName algoName = (AlgoName)navBoxForm.algoName;

        // start the selected pathfinding algo in another thread
        toaster.spawnToast(window.getSize().x / 2, "Finding a route...", "finding_route");
        std::thread([this, origin, destination, algoName]()
                    {
                        auto startTime = std::chrono::high_resolution_clock().now();
                        vector<GraphNodeIndex> path = algorithms.findShortestPath(origin, destination, algoName, mapGraph, mapGeometry, window, viewport, navBox);
                        auto endTime = std::chrono::high_resolution_clock().now();
                        // push an event with the completed route data
                        ps::Event event(ps::EventType::RouteCompleted);
                        event.data = ps::Data::CompleteRoute(path, std::chrono::duration(endTime - startTime));
                        this->eventQueue.onEvent(event); })
            .detach();
    }

    void onRouteCompleted(ps::Event event)
    {
        // In the case the a route has been found,
        // The route's edges will be displayed on the map.
        // And a message of confirmation will be displayed.

        auto data = std::get<ps::Data::CompleteRoute>(event.data);

        PointPath routePath;
        auto storage = sql::loadStorage("./db/map.db");

        // Total distance of the route in meters
        int totalDistance = 0;

        // load the point paths from all of the edges in the completed route
        for (GraphEdgeIndex idx : data.edgeIndices)
        {
            GraphEdge graphEdge = mapGraph.getEdge(idx);

            totalDistance += graphEdge.weight;

            sql::Edge edge = storage.get<sql::Edge>(graphEdge.sqlID);
            PointPath edgePath(edge.pathOffsetPoints);

            // if the edge has been duplicated in reverse, unreverse the path so that
            // it forms a continous point path from origin to destination
            if (!graphEdge.isPrimary)
            {
                edgePath.reverse();
            }
            routePath.extend(edgePath);
        }

        route.path = routePath;

        toaster.removeToast("finding_route");
        std::cout << data.edgeIndices.size() << "edges " << std::endl;

        if (totalDistance > 3000) // Convert total distance to kilometers before display.
        {
            double totalDistanceKM = totalDistance / 1000.0;
            string distanceString = to_string(totalDistanceKM);
            distanceString = distanceString.substr(0, distanceString.find(".") + 2);
            toaster.spawnToast(window.getSize().x / 2, "Route found! Have a nice trip! (" + to_string(data.runTime.count()) + ") seconds. Distance: " + distanceString + " Km.", "route_found", sf::seconds(5));
        }
        else // distance is small enough to display in meters
        {
            toaster.spawnToast(window.getSize().x / 2, "Route found! Have a nice trip! (" + to_string(data.runTime.count()) + ") seconds. Distance: " + to_string(totalDistance) + " m.", "route_found", sf::seconds(5));
        }
    }

    void clearAnimationPoints(ps::Event event)
    {
        // Clear the route and remove all dots from the map when the navbox form changes
        // Because the route no longer exists.
        route.path.clear();
        for (ChunkSprite *sprite : chunkSpriteLoader.getAllLoaded())
        {
            if (sprite->hasDots)
                chunkSpriteLoader.unCache(sprite->row, sprite->col);
        }

        // clear out any queued points representing touched nodes
        while (animationPoints.size())
            animationPoints.pop();
    }

    void enqueueAnimationPoint(ps::Event event)
    {
        // Animates a point on the map when a node is touched
        auto lonLat = std::get<ps::Data::Vector2>(event.data);
        animationPoints.push({mapGeometry.getChunkRowCol(lonLat.y, lonLat.x), sf::Vector2<double>(lonLat.x, lonLat.y)});
    }

    toml::v3::ex::parse_result config = toml::parse_file("./config/config.toml");

    sf::RenderWindow window;
    sf::Clock clock;

    Viewport viewport;
    NavBox navBox;
    Toaster toaster;
    Route route;
    Algorithms algorithms;

    ChunkLoader chunkLoader;
    ChunkSpriteLoader chunkSpriteLoader;
    MapGeometry mapGeometry;
    MapGraph mapGraph;

    std::queue<std::pair<std::pair<int, int>, sf::Vector2<double>>> animationPoints;

    ps::EventQueue eventQueue;
};