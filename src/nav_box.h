#include <SFML/Graphics.hpp>
#include "tomlplusplus/toml.hpp"
#include "node.h"
#include "viewport.h"
#include "geometry.h"
#include <iostream>

enum class AlgoName
{
    AStar,
    Dijkstras
};

class Pin {
public:
    void init(string texturePath) {
        texture.loadFromFile(texturePath);
        sprite.setTexture(texture);
        sprite.setOrigin(texture.getSize().x / 2, texture.getSize().y); // The point is the bottom tip of the pin image rather than the top-left corner;
        sprite.setScale(0.15, 0.15);
    }

    sf::Sprite& getSprite() {
        return sprite;
    }

    sf::Vector2<double> getPixelPosition() {
        return mapPosition;
    }

    sf::Vector2<double> getGeoPosition() {
        return offsetGeoPosition;
    }

    void setPixelPosition(sf::Vector2<double> position) {
        mapPosition = position;
    }

    void setGeoPosition(sf::Vector2<double> position) {
        offsetGeoPosition = position;
    }

    void setSpritePosition(sf::Vector2<double> position) {
        sprite.setPosition(position.x, position.y);
    }

private:
    sf::Texture texture;
    sf::Sprite sprite;
    sf::Vector2<double> mapPosition;
    sf::Vector2<double> offsetGeoPosition;
};

class NavBox
{
public:
    /*
    Should be called at the start of the app because it requires values that
    are not available until the App class is being constructed
    */
    void init(sf::RenderWindow* window, Viewport* viewport, MapGeometry* mapGeometry, int width, int height)
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
        font.loadFromFile("assets/fonts/Roboto-Light.ttf");
        initBackgroundBox(width, height);
        initInputBoxes(height);
        initCheckBoxes(height);
        initTextElements(height);
        initSubmitButton(height, width);
        activateOriginField();
        selectDijkstra();
        setPlaceHolders();
        initPins();
    }

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
        if (clickEvent.mouseButton.button != sf::Mouse::Left) return;

        int x = clickEvent.mouseButton.x;
        int y = clickEvent.mouseButton.y;

        if (!backgroundBox.getGlobalBounds().contains(x,y)) return;

        if (dijkstraCheckBox.getGlobalBounds().contains(x, y)) {
            selectDijkstra();
        }
        else if (aStarCheckBox.getGlobalBounds().contains(x, y)) {
            selectAStar();
        }

        // Only one field can be active at a time.
        else if (originInputBox.getGlobalBounds().contains(x,y)) {
            activateOriginField();
            deactivateDestinationField();
        }
        // An origin should be required (filled) before altering the destination
        else if (destinationInputBox.getGlobalBounds().contains(x,y)) {
            if (destinationFieldFilled || originFieldFilled)
            {
                activateDestinationField();
                deactivateOriginField();
            }
        }

        // Submitting should 'unfocus' the coordinate fields.
        else if (submitButton.getGlobalBounds().contains(x, y))
        {
            deactivateOriginField();
            deactivateDestinationField();
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

        // The active field should get a placeholder if it has been backspaced,
        // except in the case that both are empty, where only origin has a placeholder.
        if (isPressed && keyEvent.key.code == sf::Keyboard::BackSpace) {
            if (originFieldSelected && originFieldFilled) {
                originFieldFilled = false;
            }
            else if (destinationFieldSelected && originFieldFilled) {
                destinationFieldFilled = false;
                if (!originFieldFilled) {
                    originFieldSelected = true;
                    destinationFieldSelected = false;
                }
            }
            else if (destinationFieldSelected && destinationFieldFilled && !originFieldFilled) {
                destinationFieldFilled = false;
                originFieldSelected = true;
                destinationFieldSelected = false;
            }
        }

        setPlaceHolders();
    }

    /**
     * Updates the navbox elements based on mouse clicks.
     * 
     * Takes the coordinates of a click and converts them to global coordinates, then updates the input fields.
     * 
     * @param mouseEvent: Mouse clicks on the navbox elements.
    */
    void updateCoordinates(sf::Event mouseEvent) {
        // Used to update pins if the fields are filled.
        sf::Vector2<double> clickOffsetLonLat = mapGeometry->toGeoVector(
            sf::Vector2<double>(viewport->windowPositionToMapPosition(
                {mouseEvent.mouseButton.x, mouseEvent.mouseButton.y})
            )
        );

        // Used for a global coordinate system appearance within input fields.
        sf::Vector2<double> clickGlobalLonLat = mapGeometry->unoffsetGeoVector(clickOffsetLonLat);
        
        if (originFieldSelected) {
            setOriginText(clickGlobalLonLat.y, clickGlobalLonLat.x);
            updateOriginPin(clickOffsetLonLat.x, clickOffsetLonLat.y);
            deactivateOriginField();

            if (!destinationFieldFilled) {
                activateDestinationField();
            }                     
        }
        else if (destinationFieldSelected) {
            
            if (originFieldFilled) {
                setDestinationText(clickGlobalLonLat.y, clickGlobalLonLat.x);
                updateDestinationPin(clickOffsetLonLat.x, clickOffsetLonLat.y);
                deactivateDestinationField();
            }
            else {
                setOriginText(clickGlobalLonLat.y, clickGlobalLonLat.x);
                updateOriginPin(clickOffsetLonLat.x, clickOffsetLonLat.y);
                deactivateOriginField();
                deactivateDestinationField();
            }
        }

        setPlaceHolders();
    }

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
        window.draw(submitButton);
        window.draw(submitButtonLabel);

        updatePinSprites();

        if (originFieldFilled) {
            window.draw(originPin.getSprite());
        }
        if (destinationFieldFilled) {
            window.draw(destinationPin.getSprite());
        }
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

    float height;
    float width;

    ///////////////
    //UI ELEMENTS//
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

    sf::Text submitButtonLabel;
    sf::RectangleShape submitButton;

    toml::v3::ex::parse_result config = toml::parse_file("./config/config.toml");

    ///////////////////
    // UI Functions ///
    ///////////////////

    void updatePinSprites() {
        auto originPosition = originPin.getPixelPosition();
        auto destinationPosition = destinationPin.getPixelPosition();
        originPin.setSpritePosition({originPosition.x - viewport->left, originPosition.y - viewport->top});
        destinationPin.setSpritePosition({destinationPosition.x - viewport->left, destinationPosition.y - viewport->top});
    }

    void updateOriginPin(double offsetLongitude, double offsetLatitude) {
        auto pixelPosition = mapGeometry->toPixelVector({offsetLongitude, offsetLatitude});
        originPin.setGeoPosition({offsetLongitude, offsetLatitude});
        originPin.setPixelPosition(pixelPosition);
        originPin.getSprite().setPosition(pixelPosition.x - viewport->left, pixelPosition.y - viewport->top);
    }

    void updateDestinationPin(double offsetLongitude, double offsetLatitude) {
        auto position = mapGeometry->toPixelVector({offsetLongitude, offsetLatitude});
        destinationPin.setGeoPosition({offsetLongitude, offsetLatitude});
        destinationPin.setPixelPosition(position);
        destinationPin.getSprite().setPosition(position.x - viewport->left, position.y - viewport->top);
    }

    void setOriginText(const double& globalLatitude, const double& globalLongitude) {
        originText.setString(std::to_string(globalLatitude) + ", " + std::to_string(globalLongitude));
        originFieldFilled = true;
    }

    void setDestinationText(const double& globalLatitude, const double& globalLongitude) {
        destinationText.setString(std::to_string(globalLatitude) + ", " + std::to_string(globalLongitude));
        destinationFieldFilled = true;
    }

    AlgoName getSelectedAlgorithm()
    {
        return selectedAlgorithm;
    }

    void activateOriginField() {
        originInputBox.setOutlineThickness(2);
        originFieldSelected = true;
    }

    void activateDestinationField() {
        destinationInputBox.setOutlineThickness(2);
        destinationFieldSelected = true;
    }

    void deactivateDestinationField() {
        destinationInputBox.setOutlineThickness(1);
        destinationFieldSelected = false;
    }

    void deactivateOriginField() {
        originInputBox.setOutlineThickness(1);
        originFieldSelected = false;
    }

    void setPlaceHolders() {
        if (originFieldFilled && destinationFieldFilled) {
            return;
        }
        else if (originFieldFilled && !destinationFieldFilled) {
            destinationText.setString("Click on map to choose destination");
        }
        else if (!originFieldFilled && destinationFieldFilled) {
            originText.setString("Click on map to choose origin");
        }
        else {
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

    //////////////////////////
    // Initial Construction //
    //////////////////////////
    void initPins() {
        originPin.init("assets/images/pin_green.png");
        destinationPin.init("assets/images/pin_red.png");
    }

    void initBackgroundBox(const float& width, const float& height) {
        backgroundBox.setSize(sf::Vector2f(width, height));
        backgroundBox.setFillColor(sf::Color(255, 255, 255, 220));
        backgroundBox.setOutlineColor(sf::Color(255, 165, 0, 200));
        backgroundBox.setOutlineThickness(-3);
        backgroundBox.setPosition(0, window->getSize().x - height);
    }

    void initInputBoxes(float height) {
        originLabel.setFont(font);
        originLabel.setCharacterSize(15);
        originLabel.setFillColor(sf::Color::Black);
        originLabel.setString("A:");
        originLabel.setPosition(backgroundBox.getPosition().x + 10, window->getSize().x - height + 15);
        originInputBox.setSize(sf::Vector2f(backgroundBox.getSize().x - 50, 20));
        originInputBox.setFillColor(sf::Color::White);
        originInputBox.setPosition(backgroundBox.getPosition().x + 30, window->getSize().x - height + 15);
        originInputBox.setOutlineColor(sf::Color(128, 128, 128));
        originInputBox.setOutlineThickness(1);

        destinationLabel.setFont(font);
        destinationLabel.setCharacterSize(15);
        destinationLabel.setFillColor(sf::Color::Black);
        destinationLabel.setString("B: ");
        destinationLabel.setPosition(backgroundBox.getPosition().x + 10, window->getSize().x - height + 45);
        destinationInputBox.setSize(sf::Vector2f(backgroundBox.getSize().x - 50, 20));
        destinationInputBox.setFillColor(sf::Color::White);
        destinationInputBox.setPosition(backgroundBox.getPosition().x + 30, window->getSize().x - height + 45);
        destinationInputBox.setOutlineColor(sf::Color(128, 128, 128));
        destinationInputBox.setOutlineThickness(1);
    }

    void initCheckBoxes(float height) {
        dijkstraCheckBoxLabel.setFont(font);
        dijkstraCheckBoxLabel.setCharacterSize(15);
        dijkstraCheckBoxLabel.setFillColor(sf::Color::Black);
        dijkstraCheckBoxLabel.setString("Dijkstra:");
        dijkstraCheckBoxLabel.setPosition(backgroundBox.getPosition().x + 10, window->getSize().x - height + 70);
        dijkstraCheckBox.setSize(sf::Vector2f(10, 10));
        dijkstraCheckBox.setFillColor(sf::Color::White);
        dijkstraCheckBox.setOutlineColor(sf::Color(128, 128, 128));
        dijkstraCheckBox.setOutlineThickness(1);
        dijkstraCheckBox.setPosition(dijkstraCheckBoxLabel.getPosition().x + dijkstraCheckBoxLabel.getGlobalBounds().width + 5, window->getSize().x - height + 75);

        aStarCheckBoxLabel.setFont(font);
        aStarCheckBoxLabel.setCharacterSize(15);
        aStarCheckBoxLabel.setFillColor(sf::Color::Black);
        aStarCheckBoxLabel.setString("A*:");
        aStarCheckBoxLabel.setPosition(dijkstraCheckBox.getPosition().x + dijkstraCheckBox.getSize().x + 10, window->getSize().x - height + 70);
        aStarCheckBox.setSize(sf::Vector2f(10, 10));
        aStarCheckBox.setFillColor(sf::Color::White);
        aStarCheckBox.setOutlineColor(sf::Color(128, 128, 128));
        aStarCheckBox.setOutlineThickness(1);
        aStarCheckBox.setPosition(aStarCheckBoxLabel.getPosition().x + aStarCheckBoxLabel.getGlobalBounds().width + 5, window->getSize().x - height + 75);
    }

    void initTextElements(float height) {
        originText.setFont(font);
        originText.setCharacterSize(13);
        originText.setFillColor(sf::Color::Black);
        originText.setPosition(originInputBox.getPosition().x + 5, window->getSize().x - height + 17);
        destinationText.setFont(font);
        destinationText.setCharacterSize(13);
        destinationText.setFillColor(sf::Color::Black);
        destinationText.setPosition(destinationInputBox.getPosition().x + 5, window->getSize().x - height + 47);
    }

    void initSubmitButton(float height, float width) {
        submitButton.setSize(sf::Vector2f(45, 20));
        submitButton.setFillColor(sf::Color::Magenta);
        submitButton.setOutlineColor(sf::Color::Black);
        submitButton.setOutlineThickness(1);
        submitButton.setPosition(width - 60, window->getSize().x - height + 70);

        submitButtonLabel.setFont(font);
        submitButtonLabel.setCharacterSize(15);
        submitButtonLabel.setFillColor(sf::Color::Black);
        submitButtonLabel.setString("Go");
        submitButtonLabel.setPosition(width - 47, window->getSize().x - height + 70);
    }
};