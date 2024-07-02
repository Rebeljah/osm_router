#pragma once

#include "tomlplusplus/toml.hpp"

#include "geometry.h"
#include "viewport.h"
#include "chunk.h"
#include "chunk_sprite.h"


toml::v3::ex::parse_result CFG = toml::parse_file("./config/config.toml");

class App
{
public:
    App() : window(sf::VideoMode(800, 800), "OSM Router"), chunkSpriteLoader()
    {
        Degrees mapTop = *CFG["map"]["bbox_top"].value<double>();
        Degrees mapLeft = *CFG["map"]["bbox_left"].value<double>();
        Degrees mapBottom = *CFG["map"]["bbox_bottom"].value<double>();
        Degrees mapRight = *CFG["map"]["bbox_right"].value<double>();
        Degrees viewportW = *CFG["viewport"]["default_w"].value<double>();
        Degrees viewportH = *CFG["viewport"]["default_h"].value<double>();
        Degrees chunkSize = *CFG["map"]["chunk_size"].value<double>();

        pd = 800 / viewportW;

        viewport = Viewport(
            degreesToPixels(viewportW, pd),
            degreesToPixels(viewportH, pd),
            degreesToPixels(mapRight - mapLeft, pd),
            degreesToPixels(mapTop - mapBottom, pd));
        
        chunkLoader.start(&storage, chunkSize);
        chunkSpriteLoader.init(&chunkLoader, degreesToPixels(chunkSize, pd), pd);

        window.setFramerateLimit(*CFG["graphics"]["framerate"].value<int>());
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
                bool wasPressed = event.type == sf::Event::KeyPressed;

                if (event.key.code == sf::Keyboard::Key::Up)
                {
                    viewport.controlPanning(PanDirection::Up, wasPressed);
                }
                else if (event.key.code == sf::Keyboard::Key::Down)
                {
                    viewport.controlPanning(PanDirection::Down, wasPressed);
                }
                else if (event.key.code == sf::Keyboard::Key::Left)
                {
                    viewport.controlPanning(PanDirection::Left, wasPressed);
                }
                else if (event.key.code == sf::Keyboard::Key::Right)
                {
                    viewport.controlPanning(PanDirection::Right, wasPressed);
                }
            }
        }
    }

    void update()
    {
        float deltaTime = clock.restart().asSeconds();
        viewport.update(deltaTime);
    }

    void render()
    {
        window.clear();

        Pixels chunkSize = degreesToPixels(*CFG["map"]["chunk_size"].value<double>(), pd);
        int chunkRowTop = int(viewport.top / chunkSize);
        int chunkRowBottom = int(viewport.bottom() / chunkSize);
        int chunkColLeft = int(viewport.left / chunkSize);
        int chunkColRight = int(viewport.right() / chunkSize);

        for (int row = chunkRowTop - 4; row <= chunkRowBottom + 4; ++row)
        {
            for (int col = chunkColLeft - 4; col <= chunkColRight + 4; ++col)
            {
                // TODO check right and bottom bound also
                if (row < 0 || col < 0)
                {
                    continue;
                }

                auto spriteOpt = chunkSpriteLoader.get(row, col);

                if (!spriteOpt.has_value())
                    continue;

                ChunkSprite *pSprite = *spriteOpt;

                // skip drawing chunks that are buffered but not in the viewport
                if (row < chunkRowTop || row > chunkRowBottom || col < chunkColLeft || col > chunkColRight)
                    continue;

                // draw chunk
                pSprite->setPosition(pSprite->rect.left - viewport.left, pSprite->rect.top - viewport.top);
                window.draw(*pSprite);
            }
        }
        window.display();
    }

    sf::RenderWindow window;
    sf::Clock clock;

    Viewport viewport;
    ChunkLoader chunkLoader;
    ChunkSpriteLoader chunkSpriteLoader;
    double pd;

    sql::Storage storage = sql::loadStorage("./db/map.db");
};