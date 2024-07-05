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
    App() : window(sf::VideoMode(800, 800), "OSM Router"), chunkSpriteLoader()
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

        chunkLoader.start(&storage, chunkSize);
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
                bool isPressed = event.type == sf::Event::KeyPressed;

                if (event.key.code == sf::Keyboard::Key::Up)
                {
                    viewport.controlPanning(PanDirection::Up, isPressed);
                }
                else if (event.key.code == sf::Keyboard::Key::Down)
                {
                    viewport.controlPanning(PanDirection::Down, isPressed);
                }
                else if (event.key.code == sf::Keyboard::Key::Left)
                {
                    viewport.controlPanning(PanDirection::Left, isPressed);
                }
                else if (event.key.code == sf::Keyboard::Key::Right)
                {
                    viewport.controlPanning(PanDirection::Right, isPressed);
                }
            }


            /* 
            TODO: mouse-click to insert the closest nodes into origin-destination fields
                  that will be used for solving the shortest path problem.
            */ 

            if (event.type == sf::Event::MouseButtonPressed) {
                
                // Get the mouse position
                sf::Vector2i mousePos = sf::Mouse::getPosition(window);

                // Left-clicks
                if (event.mouseButton.button == sf::Mouse::Left) {

                    // Toggle the Dijkstra checkbox
                    if (navBox.getDijkstraBox().getGlobalBounds().contains(mousePos.x, mousePos.y)) {
                        navBox.toggleDijkstra();
                    }
                    // Toggle the A* search checkbox
                    else if (navBox.getAStarBox().getGlobalBounds().contains(mousePos.x, mousePos.y)) {
                        navBox.toggleAStar();
                    }

                    // TODO: If submit is clicked, find the shortest path between A and B using the 
                    // selected algorithm
                }
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
        window.clear();

        // determine the range of chunks that are inside of the viewport to render
        float chunkSize = degreesToPixels(*config["map"]["chunk_size"].value<double>(), ppd);
        int chunkRowTop = int(viewport.top / chunkSize);
        int chunkRowBottom = int(viewport.bottom() / chunkSize);
        int chunkColLeft = int(viewport.left / chunkSize);
        int chunkColRight = int(viewport.right() / chunkSize);

        for (int row = chunkRowTop - 2; row <= chunkRowBottom + 2; ++row)
        {
            for (int col = chunkColLeft - 2; col <= chunkColRight + 2; ++col)
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

    sql::Storage storage = sql::loadStorage("./db/map.db");
    toml::v3::ex::parse_result config = toml::parse_file("./config/config.toml");
};