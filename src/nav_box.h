#include <iostream>
#include <functional>

#include <SFML/Graphics.hpp>
#include "tomlplusplus/toml.hpp"

#include "viewport.h"
#include "geometry.h"
#include "pubsub.h"

enum class AlgoName
{
    AStar,
    Dijkstras
};

class Pin
{
public:
    void init(string texturePath)
    {
        texture.loadFromFile(texturePath);
        sprite.setTexture(texture);
        sprite.setOrigin(texture.getSize().x / 2, texture.getSize().y); // The point is the bottom tip of the pin image rather than the top-left corner;
        sprite.setScale(0.15, 0.15);
    }

    // Returns the sprite of the pin
    sf::Sprite &getSprite()
    {
        return sprite;
    }

    // Returns the pin's position in terms of pixels from the top left of the map.
    sf::Vector2<double> getPixelPosition()
    {
        return mapPosition;
    }

    // Returns the pin's position in terms of offset latitude and longitude from the top left of the map.
    sf::Vector2<double> getGeoPosition()
    {
        return offsetGeoPosition;
    }

    // Sets the pin's position in terms of pixels from the top left of the map.
    void setPixelPosition(sf::Vector2<double> position)
    {
        mapPosition = position;
    }

    // Sets the pin's position in terms of offset latitude and longitude from the top left of the map.
    void setGeoPosition(sf::Vector2<double> position)
    {
        offsetGeoPosition = position;
    }

    // Sets the pin's position in terms of pixels from the top left of the window.
    void setSpritePosition(sf::Vector2<double> position)
    {
        sprite.setPosition(position.x, position.y);
    }

private:
    sf::Texture texture;
    sf::Sprite sprite;
    sf::Vector2<double> mapPosition;
    sf::Vector2<double> offsetGeoPosition;
};

class NavBox : public ps::Publisher
{
public:
    /*
    Should be called at the start of the app because it requires values that
    are not available until the App class is being constructed
    */
    void init(sf::RenderWindow *window, Viewport *viewport, MapGeometry *mapGeometry, int width, int height)
    {
        // Window needs to be set before anything else, otherwise segfaults occur.
        this->window = window;
        this->viewport = viewport;
        this->mapGeometry = mapGeometry;
        this->height = height;
        this->width = width;
        originFieldSelected = true;
        destinationFieldSelected = false;
        originFieldFilled = false;
        destinationFieldFilled = false;
        animate = false;
        font.loadFromFile("assets/fonts/Roboto-Light.ttf");
        initBackgroundBox(width, height);
        initInputBoxes(height);
        initCheckBoxes(height);
        initTextElements(height);
        initSubmitButton(height, width);
        initSubmitResultText();
        activateOriginField();
        selectDijkstra();
        setPlaceHolders();
        initPins();
    }

    // Returns the background rectangle of the navbox
    sf::RectangleShape getBox()
    {
        return backgroundBox;
    }

    /**
     * Activates clicked elements of the navbox including input fields and checkboxes.
     * Shouldn't do anything unless the click was a left-click.
     *
     * @param clickEvent: Clicks on the navbox elements.
     */
    void handleClick(sf::Event clickEvent)
    {
        // Any other clicks besides left clicks are ignored.
        if (clickEvent.mouseButton.button != sf::Mouse::Left)
            return;

        int x = clickEvent.mouseButton.x;
        int y = clickEvent.mouseButton.y;

        // Clicks outside of the navbox are ignored.
        if (!backgroundBox.getGlobalBounds().contains(x, y))
            return;

        // Clicks on the checkboxes should select them.
        if (dijkstraCheckBox.getGlobalBounds().contains(x, y))
        {
            selectDijkstra();
        }
        else if (aStarCheckBox.getGlobalBounds().contains(x, y))
        {
            selectAStar();
        }
        else if (animationCheckBox.getGlobalBounds().contains(x, y))
        {
            selectAnimate();
        }
        // Only one field can be active at a time.
        else if (originInputBox.getGlobalBounds().contains(x, y))
        {
            activateOriginField();
            deactivateDestinationField();
        }
        // An origin should be required (filled) before altering the destination
        else if (destinationInputBox.getGlobalBounds().contains(x, y))
        {
            if (destinationFieldFilled || originFieldFilled)
            {
                activateDestinationField();
                deactivateOriginField();
            }
        }

        // Submitting should 'unfocus' the coordinate fields,
        // And if both fields have been filled, call the chosen algorithm
        // To calculate the shortest path.
        else if (submitButton.getGlobalBounds().contains(x, y))
        {
            deactivateOriginField();
            deactivateDestinationField();
            if (isValidSubmission())
            {
                submissionResultText.setString("");
                ps::Event event(ps::EventType::NavBoxSubmitted);
                event.data = ps::Data::NavBoxForm(offsetLonLatOrigin, offsetLonLatDestination, (int)selectedAlgorithm);
                emitEvent(event);
            }
            else
            {
                submissionResultText.setString("Error: Both fields not filled.");
            }
        }
    }

    /**
     * Updates the navbox elements based on key presses.
     *
     * @param keyEvent: Key presses on the navbox elements.
     */
    void handleKeyPress(sf::Event keyEvent)
    {
        bool isPressed = keyEvent.type == sf::Event::KeyPressed;
        bool formChanged = false;

        // The active field should get a placeholder if it has been backspaced,
        // except in the case that both are empty, where only origin has a placeholder.
        if (isPressed && keyEvent.key.code == sf::Keyboard::BackSpace)
        {
            if (originFieldSelected && originFieldFilled)
            {
                originFieldFilled = false;
                formChanged = true;
            }
            else if (destinationFieldSelected && originFieldFilled)
            {
                destinationFieldFilled = false;
                if (!originFieldFilled)
                {
                    originFieldSelected = true;
                    destinationFieldSelected = false;
                }
                formChanged = true;
            }
            else if (destinationFieldSelected && destinationFieldFilled && !originFieldFilled)
            {
                destinationFieldFilled = false;
                originFieldSelected = true;
                destinationFieldSelected = false;
                formChanged = true;
            }
        }

        if (formChanged)
            emitEvent(ps::Event(ps::EventType::NavBoxFormChanged));

        setPlaceHolders();
    }

    /**
     * Updates the navbox elements and the map pins based on mouse clicks.
     *
     * Takes the coordinates of a click and converts them to global coordinates, then updates the input fields.
     *
     * @param mouseEvent: Mouse clicks on the navbox elements.
     */
    void updateCoordinates(sf::Event mouseEvent)
    {
        // Used to update pins if the fields are filled.
        sf::Vector2<double> clickOffsetLonLat = mapGeometry->toGeoVector(
            sf::Vector2<double>(viewport->windowPositionToMapPosition(
                {static_cast<float>(mouseEvent.mouseButton.x), static_cast<float>(mouseEvent.mouseButton.y)})));

        // Used for a global coordinate system appearance within input fields.
        sf::Vector2<double> clickGlobalLonLat = mapGeometry->unoffsetGeoVector(clickOffsetLonLat);

        if (!originFieldSelected && !destinationFieldSelected)
            return;
        
        emitEvent(ps::Event(ps::EventType::NavBoxFormChanged));

        if (originFieldSelected)
        {
            setOriginText(clickGlobalLonLat.y, clickGlobalLonLat.x);
            updateOriginPin(clickOffsetLonLat.x, clickOffsetLonLat.y);
            deactivateOriginField();

            if (!destinationFieldFilled)
            {
                activateDestinationField();
            }
        }
        else if (destinationFieldSelected)
        {

            if (originFieldFilled)
            {
                setDestinationText(clickGlobalLonLat.y, clickGlobalLonLat.x);
                updateDestinationPin(clickOffsetLonLat.x, clickOffsetLonLat.y);
                deactivateDestinationField();
            }
            else
            {
                setOriginText(clickGlobalLonLat.y, clickGlobalLonLat.x);
                updateOriginPin(clickOffsetLonLat.x, clickOffsetLonLat.y);
                deactivateOriginField();
                deactivateDestinationField();
            }
        }

        setPlaceHolders();
    }

    /**
     * Draws the navbox elements on the window.
     * 
     * @param window: The window to draw the navbox elements on.
    */
    void draw(sf::RenderWindow &window)
    {
        window.draw(backgroundBox);
        window.draw(originLabel);
        window.draw(destinationLabel);
        window.draw(originInputBox);
        window.draw(originText);
        window.draw(destinationInputBox);
        window.draw(destinationText);
        window.draw(dijkstraCheckBoxLabel);
        window.draw(dijkstraCheckBox);
        window.draw(aStarCheckBoxLabel);
        window.draw(aStarCheckBox);
        window.draw(animationCheckBoxLabel);
        window.draw(animationCheckBox);
        window.draw(submitButton);
        window.draw(submitButtonLabel);
        window.draw(submissionResultText);

        updatePinSprites();

        // Should only draw the pins if the fields are filled.
        if (originFieldFilled)
        {
            window.draw(originPin.getSprite());
        }
        if (destinationFieldFilled)
        {
            window.draw(destinationPin.getSprite());
        }
    }

    // Returns the selected algorithm 
    AlgoName getSelectedAlgorithm()
    {
        return selectedAlgorithm;
    }

    // Checks if the form is valid for submission by ensuring both location fields are filled.
    bool isValidSubmission()
    {
        return (originFieldFilled && destinationFieldFilled);
    }

    // Returns the animate checkbox value the user has selected
    bool getAnimate()
    {
        return animate;
    }

private:
    Viewport *viewport;
    sf::RenderWindow *window;
    MapGeometry *mapGeometry;

    AlgoName selectedAlgorithm;
    bool originFieldSelected;
    bool destinationFieldSelected;
    bool originFieldFilled;
    bool destinationFieldFilled;
    bool animate;

    sf::Vector2<double> offsetLonLatOrigin;
    sf::Vector2<double> offsetLonLatDestination;

    float height;
    float width;

    ///////////////
    // UI ELEMENTS//
    ///////////////
    sf::RectangleShape backgroundBox;
    sf::Font font;

    sf::Text originLabel;
    sf::RectangleShape originInputBox;
    sf::Text originText;
    Pin originPin;

    sf::Text destinationText;
    Pin destinationPin;
    sf::Text destinationLabel;
    sf::RectangleShape destinationInputBox;

    sf::Text dijkstraCheckBoxLabel;
    sf::RectangleShape dijkstraCheckBox;
    sf::Text aStarCheckBoxLabel;
    sf::RectangleShape aStarCheckBox;
    sf::Text animationCheckBoxLabel;
    sf::RectangleShape animationCheckBox;

    sf::Text submitButtonLabel;
    sf::RectangleShape submitButton;

    sf::Text submissionResultText;

    toml::v3::ex::parse_result config = toml::parse_file("./config/config.toml");

    ///////////////////
    // UI Functions ///
    ///////////////////

    // Updates the pin sprites to match their tracked positions on the map.
    void updatePinSprites()
    {
        auto originPosition = originPin.getPixelPosition();
        auto destinationPosition = destinationPin.getPixelPosition();
        originPin.setSpritePosition({originPosition.x - viewport->left, originPosition.y - viewport->top});
        destinationPin.setSpritePosition({destinationPosition.x - viewport->left, destinationPosition.y - viewport->top});
    }

    // Updates the origin pin's position
    void updateOriginPin(double offsetLongitude, double offsetLatitude)
    {
        auto pixelPosition = mapGeometry->toPixelVector({offsetLongitude, offsetLatitude});
        originPin.setGeoPosition({offsetLongitude, offsetLatitude});
        originPin.setPixelPosition(pixelPosition);
        originPin.getSprite().setPosition(pixelPosition.x - viewport->left, pixelPosition.y - viewport->top);
        this->offsetLonLatOrigin = {offsetLongitude, offsetLatitude};
    }

    // Updates the destination pin's position
    void updateDestinationPin(double offsetLongitude, double offsetLatitude)
    {
        auto position = mapGeometry->toPixelVector({offsetLongitude, offsetLatitude});
        destinationPin.setGeoPosition({offsetLongitude, offsetLatitude});
        destinationPin.setPixelPosition(position);
        destinationPin.getSprite().setPosition(position.x - viewport->left, position.y - viewport->top);
        this->offsetLonLatDestination = {offsetLongitude, offsetLatitude};
    }

    // Places the origin text inside the origin input box
    void setOriginText(const double &globalLatitude, const double &globalLongitude)
    {
        originText.setString(std::to_string(globalLatitude) + ", " + std::to_string(globalLongitude));
        originFieldFilled = true;
    }

    // Places the destination text inside the destination input box
    void setDestinationText(const double &globalLatitude, const double &globalLongitude)
    {
        destinationText.setString(std::to_string(globalLatitude) + ", " + std::to_string(globalLongitude));
        destinationFieldFilled = true;
    }


    // Activates the origin field and makes the outline thicker to show that is has been selected
    void activateOriginField()
    {
        originInputBox.setOutlineThickness(2);
        originFieldSelected = true;
    }

    // Activates the destination field and makes the outline thicker to show that is has been selected
    void activateDestinationField()
    {
        destinationInputBox.setOutlineThickness(2);
        destinationFieldSelected = true;
    }

    // Deactivates the destination field and makes the outline thinner to show that is has been deselected
    void deactivateDestinationField()
    {
        destinationInputBox.setOutlineThickness(1);
        destinationFieldSelected = false;
    }

    // Deactivates the origin field and makes the outline thinner to show that is has been deselected
    void deactivateOriginField()
    {
        originInputBox.setOutlineThickness(1);
        originFieldSelected = false;
    }

    // Sets the placeholder text for the input fields
    void setPlaceHolders()
    {
        if (originFieldFilled && destinationFieldFilled)
        {
            return;
        }
        else if (originFieldFilled && !destinationFieldFilled)
        {
            destinationText.setString("Click on map to choose destination");
        }
        else if (!originFieldFilled && destinationFieldFilled)
        {
            originText.setString("Click on map to choose origin");
        }
        else
        {
            originText.setString("Click on map to choose origin");
            destinationText.setString("");
        }
    }

    // Select the Dijkstra checkbox by changing the color of the box
    void selectDijkstra()
    {
        if (selectedAlgorithm != AlgoName::Dijkstras)
        {
            dijkstraCheckBox.setFillColor(sf::Color::Black);
            aStarCheckBox.setFillColor(sf::Color::White);
            selectedAlgorithm = AlgoName::Dijkstras;
        }
    }

    // Select the A* checkbox by changing the color of the box
    void selectAStar()
    {
        if (selectedAlgorithm != AlgoName::AStar)
        {
            aStarCheckBox.setFillColor(sf::Color::Black);
            dijkstraCheckBox.setFillColor(sf::Color::White);
            selectedAlgorithm = AlgoName::AStar;
        }
    }

    // Select the animation checkbox by changing the color of the box
    void selectAnimate() 
    {
        if (animationCheckBox.getFillColor() == sf::Color::White)
        {
            animationCheckBox.setFillColor(sf::Color::Black);
            animate = true;
        }
        else
        {
            animationCheckBox.setFillColor(sf::Color::White);
            animate = false;
        }
    }

    //////////////////////////
    // Initial Construction //
    //////////////////////////
    void initPins()
    {
        originPin.init("assets/images/pin_green.png");
        destinationPin.init("assets/images/pin_red.png");
    }

    void initBackgroundBox(const float &width, const float &height)
    {
        backgroundBox.setSize(sf::Vector2f(width, height));
        backgroundBox.setFillColor(sf::Color(255, 255, 255, 220));
        backgroundBox.setOutlineColor(sf::Color(255, 165, 0, 200));
        backgroundBox.setOutlineThickness(-3);
        backgroundBox.setPosition(0, window->getSize().y - height);
    }

    void initInputBoxes(float height)
    {
        originLabel.setFont(font);
        originLabel.setCharacterSize(15);
        originLabel.setFillColor(sf::Color::Black);
        originLabel.setString("A:");
        originLabel.setPosition(backgroundBox.getPosition().x + 10, window->getSize().y - height + 15);
        originInputBox.setSize(sf::Vector2f(backgroundBox.getSize().x - 50, 20));
        originInputBox.setFillColor(sf::Color::White);
        originInputBox.setPosition(backgroundBox.getPosition().x + 30, window->getSize().y - height + 15);
        originInputBox.setOutlineColor(sf::Color(128, 128, 128));
        originInputBox.setOutlineThickness(1);

        destinationLabel.setFont(font);
        destinationLabel.setCharacterSize(15);
        destinationLabel.setFillColor(sf::Color::Black);
        destinationLabel.setString("B: ");
        destinationLabel.setPosition(backgroundBox.getPosition().x + 10, window->getSize().y - height + 45);
        destinationInputBox.setSize(sf::Vector2f(backgroundBox.getSize().x - 50, 20));
        destinationInputBox.setFillColor(sf::Color::White);
        destinationInputBox.setPosition(backgroundBox.getPosition().x + 30, window->getSize().y - height + 45);
        destinationInputBox.setOutlineColor(sf::Color(128, 128, 128));
        destinationInputBox.setOutlineThickness(1);
    }

    void initCheckBoxes(float height)
    {
        dijkstraCheckBoxLabel.setFont(font);
        dijkstraCheckBoxLabel.setCharacterSize(15);
        dijkstraCheckBoxLabel.setFillColor(sf::Color::Black);
        dijkstraCheckBoxLabel.setString("Dijkstra:");
        dijkstraCheckBoxLabel.setPosition(backgroundBox.getPosition().x + 10, window->getSize().y - height + 70);
        dijkstraCheckBox.setSize(sf::Vector2f(10, 10));
        dijkstraCheckBox.setFillColor(sf::Color::White);
        dijkstraCheckBox.setOutlineColor(sf::Color(128, 128, 128));
        dijkstraCheckBox.setOutlineThickness(1);
        dijkstraCheckBox.setPosition(dijkstraCheckBoxLabel.getPosition().x + dijkstraCheckBoxLabel.getGlobalBounds().width + 5, window->getSize().y - height + 75);

        aStarCheckBoxLabel.setFont(font);
        aStarCheckBoxLabel.setCharacterSize(15);
        aStarCheckBoxLabel.setFillColor(sf::Color::Black);
        aStarCheckBoxLabel.setString("A*:");
        aStarCheckBoxLabel.setPosition(dijkstraCheckBox.getPosition().x + dijkstraCheckBox.getSize().x + 10, window->getSize().y - height + 70);
        aStarCheckBox.setSize(sf::Vector2f(10, 10));
        aStarCheckBox.setFillColor(sf::Color::White);
        aStarCheckBox.setOutlineColor(sf::Color(128, 128, 128));
        aStarCheckBox.setOutlineThickness(1);
        aStarCheckBox.setPosition(aStarCheckBoxLabel.getPosition().x + aStarCheckBoxLabel.getGlobalBounds().width + 5, window->getSize().y - height + 75);

        animationCheckBoxLabel.setFont(font);
        animationCheckBoxLabel.setCharacterSize(15);
        animationCheckBoxLabel.setFillColor(sf::Color::Black);
        animationCheckBoxLabel.setString("Animate:");
        animationCheckBoxLabel.setPosition(dijkstraCheckBoxLabel.getPosition().x, window->getSize().y - height + 90);
        animationCheckBox.setSize(sf::Vector2f(10, 10));
        animationCheckBox.setFillColor(sf::Color::White);
        animationCheckBox.setOutlineColor(sf::Color(128, 128, 128));
        animationCheckBox.setOutlineThickness(1);
        animationCheckBox.setPosition(animationCheckBoxLabel.getPosition().x + animationCheckBoxLabel.getGlobalBounds().width + 5, window->getSize().y - height + 95);
    }

    void initTextElements(float height)
    {
        originText.setFont(font);
        originText.setCharacterSize(13);
        originText.setFillColor(sf::Color::Black);
        originText.setPosition(originInputBox.getPosition().x + 5, window->getSize().y - height + 17);
        destinationText.setFont(font);
        destinationText.setCharacterSize(13);
        destinationText.setFillColor(sf::Color::Black);
        destinationText.setPosition(destinationInputBox.getPosition().x + 5, window->getSize().y - height + 47);
    }

    void initSubmitButton(float height, float width)
    {
        submitButton.setSize(sf::Vector2f(45, 20));
        submitButton.setFillColor(sf::Color::Magenta);
        submitButton.setOutlineColor(sf::Color::Black);
        submitButton.setOutlineThickness(1);
        submitButton.setPosition(width - 65, window->getSize().y - height + 70);

        submitButtonLabel.setFont(font);
        submitButtonLabel.setCharacterSize(15);
        submitButtonLabel.setFillColor(sf::Color::Black);
        submitButtonLabel.setString("Go");
        submitButtonLabel.setPosition(width - 52, window->getSize().y - height + 70);
    }

    void initSubmitResultText()
    {
        submissionResultText.setFont(font);
        submissionResultText.setCharacterSize(13);
        submissionResultText.setFillColor(sf::Color::Black);
        submissionResultText.setPosition(backgroundBox.getPosition().x + 10, window->getSize().y - height + 95);
    }
};