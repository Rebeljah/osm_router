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
    App() : window(sf::VideoMode(800, 800), "NaviGator")
    {
        using Degree = double;
        Degree mapTop = *config["map"]["bbox_top"].value<double>();
        Degree mapLeft = *config["map"]["bbox_left"].value<double>();
        Degree mapBottom = *config["map"]["bbox_bottom"].value<double>();
        Degree mapRight = *config["map"]["bbox_right"].value<double>();
        Degree viewportW = *config["viewport"]["default_w"].value<double>();
        Degree viewportH = *config["viewport"]["default_h"].value<double>();
        Degree chunkSize = *config["map"]["chunk_size"].value<double>();

        // connect the custom event queue to listen to the navbox event(s)
        eventQueue.subscribe(&navBox, ps::EventType::NavBoxSubmitted);
        
        // load map data in background. Done event will be handled in event loop.
        std::thread([this](){
            this->mapGraph.load("./db/map.db");
            this->eventQueue.pushEvent(ps::Event(ps::EventType::MapDataLoaded));
        }).detach();

        mapGeometry = MapGeometry(
            800 / viewportW,  // pixels per degree
            { mapTop, mapLeft, mapRight - mapLeft, mapTop - mapBottom },  // map geo area
            chunkSize // chunk geo size
        );

        route.mapGeometry = &mapGeometry;

        sf::Vector2<double> viewportGeoSize = {
             *config["viewport"]["default_w"].value<double>(),
             *config["viewport"]["default_h"].value<double>()
        };

        viewport = Viewport(mapGeometry.toPixelVector(viewportGeoSize), &mapGeometry);
        
        viewport.centerOnPoint(
            mapGeometry.toPixelVector(
                mapGeometry.offsetGeoVector({ -82.325005, 29.651982 })  // Gville, FL
            )
        ); 

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

            if (event.type == sf::Event::MouseButtonPressed)
            {
                int x = event.mouseButton.x;
                int y = event.mouseButton.y;

                if (navBox.getBox().getGlobalBounds().contains(x, y))
                    navBox.handleClick(event);
                else {
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
        while (!eventQueue.empty())
        {
            ps::Event event = eventQueue.popNext();

            if (event.type == ps::EventType::MapDataLoaded)
            {
                toaster.spawnToast(window.getSize().x / 2, "Map data loaded! Let's go!", "loading_data", sf::seconds(3));
            }
            else if (event.type == ps::EventType::NavBoxSubmitted)
            {
                if (!mapGraph.isDataLoaded())
                {
                    toaster.spawnToast(window.getSize().x / 2, "Loading data, please wait...", "loading_data", sf::seconds(2.25));
                    continue;
                }

                auto navBoxForm = std::get<ps::Data::NavBoxForm>(event.data);
                sf::Vector2<double> origin = navBoxForm.origin;
                sf::Vector2<double> destination = navBoxForm.destination;
                AlgoName algoName = (AlgoName)navBoxForm.algoName;

                toaster.spawnToast(window.getSize().x/2, "Finding a route...", "finding_route");
                std::thread([this, origin, destination, algoName]()
                    {
                        auto startTime = std::chrono::high_resolution_clock().now();
                        vector<GraphNodeIndex> path = findShortestPath(origin, destination, algoName, mapGraph, mapGeometry);
                        auto endTime = std::chrono::high_resolution_clock().now();
                        // push an event with the completed route data
                        ps::Event event(ps::EventType::RouteCompleted);
                        event.data = ps::Data::CompleteRoute(path, std::chrono::duration(endTime - startTime));
                        this->eventQueue.onEvent(event);
                    }).detach();
            }
            else if (event.type == ps::EventType::RouteCompleted)
            {
                auto data = std::get<ps::Data::CompleteRoute>(event.data);

                PointPath routePath;

                auto storage = sql::loadStorage("./db/map.db");
                for (GraphEdgeIndex idx : data.edgeIndices)
                {
                    GraphEdge graphEdge = mapGraph.getEdge(idx);
                    sql::Edge edge = storage.get<sql::Edge>(graphEdge.sqlID);
                    PointPath edgePath(edge.pathOffsetPoints);
                    if (!graphEdge.isPrimary)
                    {
                        edgePath.reverse();
                    }
                    routePath.extend(edgePath);
                }

                route.path = routePath;

                toaster.removeToast("finding_route");
                std::cout << data.edgeIndices.size() << "edges " << std::endl;
                toaster.spawnToast(window.getSize().x / 2, "Route found! Have a nice trip! (" + to_string(data.runTime.count()) + ") seconds", "route_found", sf::seconds(7));
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
        window.clear(sf::Color(247, 246, 246, 255));

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

        // Draws a navigation box at the bottom-left of the screen
        navBox.draw(window);
        toaster.render(window);
        route.render(window, (Rectangle<double>)viewport);
        window.display();
    }

    toml::v3::ex::parse_result config = toml::parse_file("./config/config.toml");

    sf::RenderWindow window;
    sf::Clock clock;

    Viewport viewport;
    NavBox navBox;
    Toaster toaster;
    Route route;

    ChunkLoader chunkLoader;
    ChunkSpriteLoader chunkSpriteLoader;
    MapGeometry mapGeometry;
    MapGraph mapGraph;

    ps::EventQueue eventQueue;
};