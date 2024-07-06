#pragma once

#include "tomlplusplus/toml.hpp"

#include "geometry.h"
#include "viewport.h"
#include "chunk.h"
#include "chunk_sprite.h"
#include "nav_box.h"

class App
{
public:
    App() : window(sf::VideoMode(800, 800), "NaviGator"), chunkSpriteLoader()
    {
        using Degree = double;
        Degree mapTop = *config["map"]["bbox_top"].value<double>();
        Degree mapLeft = *config["map"]["bbox_left"].value<double>();
        Degree mapBottom = *config["map"]["bbox_bottom"].value<double>();
        Degree mapRight = *config["map"]["bbox_right"].value<double>();
        Degree viewportW = *config["viewport"]["default_w"].value<double>();
        Degree viewportH = *config["viewport"]["default_h"].value<double>();
        Degree chunkSize = *config["map"]["chunk_size"].value<double>();

        // ratio of pixels per degree
        ppd = 800 / viewportW;

        viewport = Viewport(
            degreesToPixels(viewportW, ppd),
            degreesToPixels(viewportH, ppd),
            degreesToPixels(mapRight - mapLeft, ppd),
            degreesToPixels(mapTop - mapBottom, ppd));

        navBox.init(250, 140, window);

        chunkLoader.start(chunkSize, "./db/map.db");
        chunkSpriteLoader.init(&chunkLoader, degreesToPixels(chunkSize, ppd), ppd);

        window.setFramerateLimit(*config["graphics"]["framerate"].value<int>());
    }

    ~App()
    {
    }

    void run()
    {
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
    }

    void render()
    {
        window.clear(sf::Color(247, 246, 246, 255));

        // determine the range of chunks that are inside of the viewport to render
        float chunkSize = degreesToPixels(*config["map"]["chunk_size"].value<double>(), ppd);
        int chunkRowTop = int(viewport.top / chunkSize);
        int chunkRowBottom = int(viewport.bottom() / chunkSize);
        int chunkColLeft = int(viewport.left / chunkSize);
        int chunkColRight = int(viewport.right() / chunkSize);

        for (int row = chunkRowTop - 1; row <= chunkRowBottom + 1; ++row)
        {
            for (int col = chunkColLeft - 1; col <= chunkColRight + 1; ++col)
            {
                // prevents rendering chunks that are out of bounds
                // TODO check right and bottom bound also
                if (row < 0 || col < 0)
                {
                    continue;
                }

                // retrieve the chunk sprite if it is already rendered
                // if sprite is not rendered, the option will not have a value, so
                // the loop continues to the next chunk
                auto spriteOpt = chunkSpriteLoader.get(row, col);
                if (!spriteOpt.has_value())
                    continue;

                ChunkSprite &sprite = **spriteOpt;

                // skip drawing chunks that are buffered but not in the viewport
                if (row < chunkRowTop || row > chunkRowBottom || col < chunkColLeft || col > chunkColRight)
                    continue;

                // draw chunk
                sprite.setPosition(sprite.rect.left - viewport.left, sprite.rect.top - viewport.top);
                window.draw(sprite);
            }
        }

        // Draws a navigation box at the bottom-left of the screen
        navBox.draw(window);

        window.display();
    }

    sf::RenderWindow window;
    sf::Clock clock;

    Viewport viewport;
    NavBox navBox;
    ChunkLoader chunkLoader;
    ChunkSpriteLoader chunkSpriteLoader;
    double ppd;
    toml::v3::ex::parse_result config = toml::parse_file("./config/config.toml");
};