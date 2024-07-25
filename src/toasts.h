#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <SFML/Graphics.hpp>

using std::string;
using std::unordered_map;
using std::vector;

/**
 * Represents a toast message displayed on screen.
 */
class Toast : public sf::Sprite
{
public:
    /**
     * Constructor to create a new Toast.
     *
     * @param message The message text to display.
     * @param font The font to use for the message text.
     * @param centerX The x-coordinate where the toast should be centered.
     */
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
        setPosition(centerX, -1);                    // mid top of window

        // pos after panning into view
        finalPosition = sf::Vector2f(getPosition().x, marginY + surfaceSize.y);

        panVelocity = (finalPosition.y - getPosition().y) / 1; // pixels / sec
    }

    /**
     * Initiates the animation to pan the toast into view.
     */
    void spawn()
    {
        isPanningIn = true;
        isPanningOut = false;
    }

    /**
     * Initiates the animation to pan the toast out of view and marks it for removal.
     */
    void remove()
    {
        isPanningOut = true;
        isPanningIn = false;
        wasRemoved = true;
    }

    /**
     * Checks if the toast has been removed.
     *
     * @return true if the toast has been removed and finished panning out.
     */
    bool isRemoved()
    {
        return wasRemoved && !isPanningOut;
    }

    /**
     * Updates the toast's position based on the elapsed time.
     *
     * @param deltaTime The time elapsed since the last update.
     */
    void update(const float &deltaTime)
    {
        if (isPanningIn)
        {
            setPosition(getPosition() + sf::Vector2f(0, panVelocity * deltaTime));
            if (getPosition().y >= finalPosition.y)
                setPosition(finalPosition);
                isPanningIn = false;
        }
        else if (isPanningOut)
        {
            setPosition(getPosition() - sf::Vector2f(0, panVelocity * deltaTime));
            if (getPosition().y < 0)
            {
                setPosition(getPosition().x, -1);
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

struct ToastLifetime
{
    ToastLifetime(sf::Time spawnTime, sf::Time timeToLive, const sf::Clock &clock)
        : spawnTime(spawnTime), timeToLive(timeToLive), clock(clock)
    {
    }

    const sf::Time spawnTime;
    const sf::Time timeToLive;
    const sf::Clock &clock;

    /**
     * Determine whether the toast has been alive longer than its time-to-live
     */
    bool isExpired() const
    {
        return (clock.getElapsedTime() - spawnTime) > timeToLive;
    }
};

/**
 * Manages your toasts for you :).
 */
class Toaster
{
public:
    Toaster()
    {
        font.loadFromFile("./assets/fonts/Roboto-Light.ttf");
    }

    ~Toaster()
    {
        for (auto [_, toast] : toasts)
        {
            delete toast;
        }
    }

    /**
     * Spawns a new toast message at a specified position.
     *
     * @param centerX The x-coordinate where the toast should be centered.
     * @param message The message text to display.
     * @param toastID The unique identifier for the toast.
     * @param timeToLive duration after which the toast will automatically be removed.
     */
    void spawnToast(int centerX, string message, string toastID, sf::Time timeToLive = sf::seconds(99999))
    {
        if (toasts.find(toastID) != toasts.end())
            return; // non-unique id

        auto t = new Toast(message, font, centerX);
        toasts.emplace(toastID, t);
        toastLifetimes.emplace(toastID, ToastLifetime(clock.getElapsedTime(), timeToLive, clock));
        t->spawn();
    }

    /**
     * Initiates the removal animation for a toast message.
     *
     * @param toastID The unique identifier of the toast message to remove.
     */
    void removeToast(string toastID)
    {
        auto iterator = toasts.find(toastID);

        if (iterator == toasts.end()) // ID doesn't exist
            return;

        iterator->second->remove();
    }

    /**
     * Updates all active toast messages based on elapsed time.
     *
     * @param deltaTime The time elapsed since the last update.
     */
    void update(const float &deltaTime)
    {
        vector<std::pair<const string &, Toast *>> removedToasts;

        for (auto &[id, toast] : toasts)
        {
            toast->update(deltaTime);

            if (toastLifetimes.at(id).isExpired())
                removeToast(id);

            if (toast->isRemoved())
                removedToasts.push_back({id, toast});
        }

        for (auto &[id, toast] : removedToasts)
        {
            delete toast;
            toasts.erase(id);
            toastLifetimes.erase(id);
        }
    }

    /**
     * Renders all active toast messages onto the given render window.
     *
     * @param window The render window to render the toasts onto.
     */
    void render(sf::RenderWindow &window)
    {
        for (auto &[id, toast] : toasts)
        {
            window.draw(*toast);
        }
    }

private:
    sf::Font font;

    sf::Clock clock;
    unordered_map<string, Toast *> toasts;
    unordered_map<string, ToastLifetime> toastLifetimes;
};
