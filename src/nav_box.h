#include <SFML/Graphics.hpp>

enum class AlgoName
{
    AStar,
    Dijkstras
};

class NavBox
{
public:
    NavBox()
    {
    }

    /*
    Should be called at the start of the app because it requires values that
    are not available until the App class is being constructed
    */
    void init(float width, float height, sf::RenderWindow &window)
    {
        backgroundBox.setSize(sf::Vector2f(width, height));
        backgroundBox.setFillColor(sf::Color(255, 255, 255, 220));
        backgroundBox.setOutlineColor(sf::Color(255, 165, 0, 200));
        backgroundBox.setOutlineThickness(-3);
        backgroundBox.setPosition(0, window.getSize().y - height);

        font.loadFromFile("assets/fonts/Roboto-Light.ttf");

        originLabel.setFont(font);
        originLabel.setCharacterSize(15);
        originLabel.setFillColor(sf::Color::Black);
        originLabel.setString("A:");
        originLabel.setPosition(backgroundBox.getPosition().x + 10, window.getSize().y - height + 40);

        originInputBox.setSize(sf::Vector2f(backgroundBox.getSize().x - 50, 20));
        originInputBox.setFillColor(sf::Color::White);
        originInputBox.setPosition(backgroundBox.getPosition().x + 30, window.getSize().y - height + 40);
        originInputBox.setOutlineColor(sf::Color(128, 128, 128));
        originInputBox.setOutlineThickness(1);

        destinationLabel.setFont(font);
        destinationLabel.setCharacterSize(15);
        destinationLabel.setFillColor(sf::Color::Black);
        destinationLabel.setString("B: ");
        destinationLabel.setPosition(backgroundBox.getPosition().x + 10, window.getSize().y - height + 70);

        destinationInputBox.setSize(sf::Vector2f(backgroundBox.getSize().x - 50, 20));
        destinationInputBox.setFillColor(sf::Color::White);
        destinationInputBox.setPosition(backgroundBox.getPosition().x + 30, window.getSize().y - height + 70);
        destinationInputBox.setOutlineColor(sf::Color(128, 128, 128));
        destinationInputBox.setOutlineThickness(1);

        dijkstraCheckBoxLabel.setFont(font);
        dijkstraCheckBoxLabel.setCharacterSize(15);
        dijkstraCheckBoxLabel.setFillColor(sf::Color::Black);
        dijkstraCheckBoxLabel.setString("Dijkstra:");
        dijkstraCheckBoxLabel.setPosition(backgroundBox.getPosition().x + 10, window.getSize().y - height + 100);

        dijkstraCheckBox.setSize(sf::Vector2f(10, 10));
        dijkstraCheckBox.setFillColor(sf::Color::White);
        dijkstraCheckBox.setOutlineColor(sf::Color(128, 128, 128));
        dijkstraCheckBox.setOutlineThickness(1);
        dijkstraCheckBox.setPosition(dijkstraCheckBoxLabel.getPosition().x + dijkstraCheckBoxLabel.getGlobalBounds().width + 5, window.getSize().y - height + 105);

        aStarCheckBoxLabel.setFont(font);
        aStarCheckBoxLabel.setCharacterSize(15);
        aStarCheckBoxLabel.setFillColor(sf::Color::Black);
        aStarCheckBoxLabel.setString("A*:");
        aStarCheckBoxLabel.setPosition(dijkstraCheckBox.getPosition().x + dijkstraCheckBox.getSize().x + 10, window.getSize().y - height + 100);

        aStarCheckBox.setSize(sf::Vector2f(10, 10));
        aStarCheckBox.setFillColor(sf::Color::White);
        aStarCheckBox.setOutlineColor(sf::Color(128, 128, 128));
        aStarCheckBox.setOutlineThickness(1);
        aStarCheckBox.setPosition(aStarCheckBoxLabel.getPosition().x + aStarCheckBoxLabel.getGlobalBounds().width + 5, window.getSize().y - height + 105);

        originText.setFont(font);
        originText.setCharacterSize(13);
        originText.setFillColor(sf::Color::Black);
        originText.setPosition(originInputBox.getPosition().x + 5, window.getSize().y - height + 42);

        destinationText.setFont(font);
        destinationText.setCharacterSize(13);
        destinationText.setFillColor(sf::Color::Black);
        destinationText.setPosition(destinationInputBox.getPosition().x + 5, window.getSize().y - height + 72);

        submitButton.setSize(sf::Vector2f(45, 20));
        submitButton.setFillColor(sf::Color::Magenta);
        submitButton.setOutlineColor(sf::Color::Black);
        submitButton.setOutlineThickness(1);
        submitButton.setPosition(width - 60, window.getSize().y - height + 100);

        submitButtonLabel.setFont(font);
        submitButtonLabel.setCharacterSize(15);
        submitButtonLabel.setFillColor(sf::Color::Black);
        submitButtonLabel.setString("Go");
        submitButtonLabel.setPosition(width - 47, window.getSize().y - height + 100);

        // set the default algorithm selection to Dijkstra's
        selectDijkstra();
    }

    sf::RectangleShape getBox()
    {
        return backgroundBox;
    }

    void handleClick(sf::Event clickEvent)
    {

        if (clickEvent.mouseButton.button != sf::Mouse::Left)
            return;

        int x = clickEvent.mouseButton.x;
        int y = clickEvent.mouseButton.y;

        if (dijkstraCheckBox.getGlobalBounds().contains(x, y))
        {
            selectDijkstra();
        }
        else if (aStarCheckBox.getGlobalBounds().contains(x, y))
        {
            selectAStar();
        }
        else if (submitButton.getGlobalBounds().contains(x, y))
        {
        }
    }

    // Changes the text within the origin text field
    void setOriginText(sf::String text)
    {
        originText.setString(text);
    }

    // Changes the text within the destination text field
    void setDestinationText(sf::String text)
    {
        destinationText.setString(text);
    }

    AlgoName getSelectedAlgorithm()
    {
        return selectedAlgorithm;
    }

    // Draw all elements of the NavBox
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
    }

private:
    AlgoName selectedAlgorithm;

    ///////////////
    //UI ELEMENTS//
    ///////////////
    sf::RectangleShape backgroundBox;
    sf::Font font;

    sf::Text originLabel;
    sf::RectangleShape originInputBox;

    sf::Text destinationLabel;
    sf::RectangleShape destinationInputBox;

    sf::Text dijkstraCheckBoxLabel;
    sf::RectangleShape dijkstraCheckBox;
    sf::Text aStarCheckBoxLabel;
    sf::RectangleShape aStarCheckBox;

    sf::Text originText;
    sf::Text destinationText;

    sf::Text submitButtonLabel;
    sf::RectangleShape submitButton;

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
};