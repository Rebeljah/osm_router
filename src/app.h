#pragma once

#include <thread>
#include <chrono>

#include "tomlplusplus/toml.hpp"

#include "geometry.h"
#include "viewport.h"
#include "chunk_sprite.h"
#include "nav_box.h"
#include "toasts.h"

#include "graph.h"

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

        mapGraph.load("./db/map.db");

        mapGeometry = MapGeometry(
            800 / viewportW,  // pixels per degree
            { mapTop, mapLeft, mapRight - mapLeft, mapTop - mapBottom },  // map geo area
            chunkSize // chunk geo size
        );

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

        navBox.init(250, 140, window);

        chunkSpriteLoader.init(&mapGeometry, "./db/map.db");

        window.setFramerateLimit(*config["graphics"]["framerate"].value<int>());

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

            // pan the map around by holding arrow keys
            if (event.type == sf::Event::KeyPressed || event.type == sf::Event::KeyReleased)
            {
                viewport.controlPanning(event);
            }

            /*
            TODO: mouse-click to insert the closest nodes into origin-destination fields
                  that will be used for solving the shortest path problem.
            */

            if (event.type == sf::Event::MouseButtonPressed)
            {
                int x = event.mouseButton.x;
                int y = event.mouseButton.y;

                if (navBox.getBox().getGlobalBounds().contains(x, y))
                    navBox.handleClick(event);
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

        window.display();
    }

    toml::v3::ex::parse_result config = toml::parse_file("./config/config.toml");

    sf::RenderWindow window;
    sf::Clock clock;

    Viewport viewport;
    NavBox navBox;
    Toaster toaster;

    ChunkLoader chunkLoader;
    ChunkSpriteLoader chunkSpriteLoader;
    MapGeometry mapGeometry;
    MapGraph mapGraph;
};