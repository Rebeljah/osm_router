#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <SFML/Graphics.hpp>

#include "geometry.h"

using std::string;
using std::unordered_map;
using std::vector;

class Toast : public sf::Sprite
{
public:
    Toast(const string &message, const sf::Font &font, const int &centerX)
    {
        int padX = 20;    // internal space between toast border and text
        int padY = 15;    // ...
        int marginY = 20; // external spacing from window border

        // create text drawable before surface texture to determine required size
        auto text = sf::Text(message, font, 12);
        text.setFillColor(sf::Color::Black);
        auto textSize = sf::Vector2f(text.getLocalBounds().width, text.getLocalBounds().height);
        auto surfaceSize = sf::Vector2u(textSize.x + padX * 2, textSize.y + padY * 2);

        // center text
        text.setOrigin({textSize.x / 2, textSize.y / 2});
        text.setPosition(surfaceSize.x / 2, surfaceSize.y / 2);

        auto background = sf::RectangleShape(sf::Vector2f(surfaceSize));
        background.setFillColor(sf::Color::White);
        background.setOutlineColor(sf::Color::Black);
        background.setOutlineThickness(-2);

        surface.create(surfaceSize.x, surfaceSize.y);
        surface.clear(sf::Color::Transparent);
        surface.draw(background);
        surface.draw(text);
        surface.display();
        setTexture(surface.getTexture());

        // initial pos will be just above visible window area
        setOrigin(surfaceSize.x / 2, surfaceSize.y); // mid bottom
        setPosition(centerX, -1);  // mid top of window

        // pos after panning into view
        finalPosition = sf::Vector2f(getPosition().x, marginY + surfaceSize.y);

        panVelocity = (finalPosition.y - getPosition().y) / 1; // pixels / sec
    }

    void spawn()
    {
        isPanningIn = true;
        isPanningOut = false;
    }

    void remove()
    {
        isPanningOut = true;
        isPanningIn = false;
        wasRemoved = true;
    }

    bool isRemoved()
    {
        return wasRemoved && !isPanningOut;
    }

    void update(const float &deltaTime)
    {
        if (isPanningIn)
        {
            setPosition(getPosition().x, getPosition().y + panVelocity * deltaTime);
            if (getPosition().y >= finalPosition.y)
                isPanningIn = false;
        }
        else if (isPanningOut)
        {
            setPosition(getPosition().x, getPosition().y - panVelocity * deltaTime);
            if (getPosition().y < 0)
            {
                isPanningOut = false;
            }
        }
    }

private:
    sf::RenderTexture surface;
    sf::Vector2f finalPosition;
    bool isPanningIn = false;
    bool isPanningOut = false;
    bool wasRemoved = false;
    float panVelocity;
};

class Toaster
{
public:
    Toaster()
    {
        font.loadFromFile("./assets/fonts/Roboto-Light.ttf");
    }

    void spawnToast(int centerX, string message, string toastID)
    {
        auto t = new Toast(message, font, centerX);
        toasts.emplace(toastID, t);
        t->spawn();
    }

    void removeToast(string toastID)
    {
        auto iterator = toasts.find(toastID);

        if (iterator == toasts.end()) // ID doesn't exist
            return;

        iterator->second->remove();
    }

    void update(const float &deltaTime)
    {
        vector<std::pair<const string &, Toast *>> removedToasts;

        for (auto &[id, toast] : toasts)
        {
            toast->update(deltaTime);

            if (toast->isRemoved())
                removedToasts.push_back({id, toast});
        }

        for (auto &[id, toast] : removedToasts)
        {
            delete toast;
            toasts.erase(id);
        }
    }

    void render(sf::RenderWindow &window)
    {
        for (auto &[id, toast] : toasts)
        {
            window.draw(*toast);
        }
    }

private:
    sf::Font font;
    Rectangle<int> windowSize;
    unordered_map<string, Toast *> toasts;
};