#include <SFML/Graphics.hpp>

// Class for the navigation box at the bottom of the window
class NavBox {
private:
    // Basic box elements
    sf::RectangleShape box;
    sf::Text title;
    sf::Font font;

    // Text fields for origin and destination
    sf::Text origin;
    sf::RectangleShape originBox;

    sf::Text destination;
    sf::RectangleShape destinationBox;

    // Checkboxes for selecting the algorithm
    sf::RectangleShape dijkstraBox;
    sf::Text dijkstra;
    sf::RectangleShape aStarBox;
    sf::Text aStar;

    // Contains the selected algorithm which can later be checked by the algorithm trigger
    std::string selectedAlgorithm;

    // Strings for the origin and destination text fields
    sf::String originInput;
    sf::String desintationInput;
    sf::Text originInputText;
    sf::Text destinationInputText;

    // Button for solving the shortest path problem
    sf::Text submit;
    sf::RectangleShape submitBox;

public:
    // Default constructor
    NavBox() {}

    // Constructs the box
    void init(float width, float height, sf::RenderWindow& window) {
        box.setSize(sf::Vector2f(width, height));
        box.setFillColor(sf::Color(255, 255, 255, 220));
        box.setOutlineColor(sf::Color(255, 165, 0, 200));
        box.setOutlineThickness(-3);
        box.setPosition(0, window.getSize().y - height);

        // Set the title
        font.loadFromFile("assets/fonts/Roboto-Light.ttf");
        title.setFont(font);
        title.setCharacterSize(18);
        title.setFillColor(sf::Color::Black);
        title.setString("NavBox");
        title.setPosition((width - title.getGlobalBounds().width) / 2, window.getSize().y - height + 7);

        // Set the origin text field
        origin.setFont(font);
        origin.setCharacterSize(15);
        origin.setFillColor(sf::Color::Black);
        origin.setString("A:");
        origin.setPosition(box.getPosition().x + 10, window.getSize().y - height + 40);

        // Set a white background for the text field next to the origin text
        originBox.setSize(sf::Vector2f(box.getSize().x - 50, 20));
        originBox.setFillColor(sf::Color::White);
        originBox.setPosition(box.getPosition().x + 30, window.getSize().y - height + 40);
        originBox.setOutlineColor(sf::Color(128, 128, 128));
        originBox.setOutlineThickness(1);

        // Set the destination text field
        destination.setFont(font);
        destination.setCharacterSize(15);
        destination.setFillColor(sf::Color::Black);
        destination.setString("B: ");
        destination.setPosition(box.getPosition().x + 10, window.getSize().y - height + 70);

        // Set a white background for the text field next to the destination text
        destinationBox.setSize(sf::Vector2f(box.getSize().x - 50, 20));
        destinationBox.setFillColor(sf::Color::White);
        destinationBox.setPosition(box.getPosition().x + 30, window.getSize().y - height + 70);
        destinationBox.setOutlineColor(sf::Color(128, 128, 128));
        destinationBox.setOutlineThickness(1);

        // Set the Dijkstra checkbox at the bottom of the box
        dijkstra.setFont(font);
        dijkstra.setCharacterSize(15);
        dijkstra.setFillColor(sf::Color::Black);
        dijkstra.setString("Dijkstra:");
        dijkstra.setPosition(box.getPosition().x + 10, window.getSize().y - height + 100);

        // Set a white background for the checkbox next to the Dijkstra text
        dijkstraBox.setSize(sf::Vector2f(10, 10));
        dijkstraBox.setFillColor(sf::Color::White);
        dijkstraBox.setOutlineColor(sf::Color(128, 128, 128));
        dijkstraBox.setOutlineThickness(1);
        dijkstraBox.setPosition(dijkstra.getPosition().x + dijkstra.getGlobalBounds().width + 5, window.getSize().y - height + 105);

        // Set the A* checkbox and background to the right of the Dijkstra checkbox
        aStar.setFont(font);
        aStar.setCharacterSize(15);
        aStar.setFillColor(sf::Color::Black);
        aStar.setString("A*:");
        aStar.setPosition(dijkstraBox.getPosition().x + dijkstraBox.getSize().x + 10, window.getSize().y - height + 100);

        // Set a white background for the checkbox next to the A* text
        aStarBox.setSize(sf::Vector2f(10, 10));
        aStarBox.setFillColor(sf::Color::White);
        aStarBox.setOutlineColor(sf::Color(128, 128, 128));
        aStarBox.setOutlineThickness(1);
        aStarBox.setPosition(aStar.getPosition().x + aStar.getGlobalBounds().width + 5, window.getSize().y - height + 105);

        // Set the input text to be empty by default
        originInput = "";
        originInputText.setFont(font);
        originInputText.setCharacterSize(13);
        originInputText.setFillColor(sf::Color::Black);
        originInputText.setString(originInput);
        originInputText.setPosition(originBox.getPosition().x + 5, window.getSize().y - height + 42);

        desintationInput = "";
        destinationInputText.setFont(font);
        destinationInputText.setCharacterSize(13);
        destinationInputText.setFillColor(sf::Color::Black);
        destinationInputText.setString(desintationInput);
        destinationInputText.setPosition(destinationBox.getPosition().x + 5, window.getSize().y - height + 72);

        // Set the submit button at the bottom of the box
        submitBox.setSize(sf::Vector2f(45, 20));
        submitBox.setFillColor(sf::Color::Magenta);
        submitBox.setOutlineColor(sf::Color::Black);
        submitBox.setOutlineThickness(1);
        submitBox.setPosition(width - 60, window.getSize().y - height + 100);

        submit.setFont(font);
        submit.setCharacterSize(15);
        submit.setFillColor(sf::Color::Black);
        submit.setString("Go");
        submit.setPosition(width - 47, window.getSize().y - height + 100);

        selectedAlgorithm = "";
    }

    // Changes the text within the origin text field
    void setOriginText(sf::String text) {
        originInput = text;
        originInputText.setString(originInput);
    }

    // Changes the text within the destination text field
    void setDestinationText(sf::String text) {
        desintationInput = text;
        destinationInputText.setString(desintationInput);
    }

    // Returns dijkstraBox
    sf::RectangleShape getDijkstraBox() {
        return dijkstraBox;
    }

    // Returns aStarBox
    sf::RectangleShape getAStarBox() {
        return aStarBox;
    }

    // Toggle the Dijkstra checkbox by changing the color of the box
    void toggleDijkstra() {
        if (dijkstraBox.getFillColor() == sf::Color::White) {
            dijkstraBox.setFillColor(sf::Color::Black);
            selectedAlgorithm = "Dijkstra";
            aStarBox.setFillColor(sf::Color::White);
        } else {
            dijkstraBox.setFillColor(sf::Color::White);
        }
    }

    // Toggle the A* checkbox by changing the color of the box
    void toggleAStar() {
        if (aStarBox.getFillColor() == sf::Color::White) {
            aStarBox.setFillColor(sf::Color::Black);
            selectedAlgorithm = "A*";
            dijkstraBox.setFillColor(sf::Color::White);
        } else {
            aStarBox.setFillColor(sf::Color::White);
        }
    }
    
    // Draw all elements of the NavBox
    void draw(sf::RenderWindow& window) {
        window.draw(box);
        window.draw(title);
        window.draw(origin);
        window.draw(destination);
        window.draw(originBox);
        window.draw(originInputText);
        window.draw(destinationBox);
        window.draw(destinationInputText);
        window.draw(dijkstra);
        window.draw(dijkstraBox);
        window.draw(aStar);
        window.draw(aStarBox);
        window.draw(submitBox);
        window.draw(submit);
    }
};