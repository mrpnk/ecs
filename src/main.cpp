#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include "ecs.hpp"
#include "framerate.hpp"
#include <chrono>

class Game{

	sf::Texture texture;
	sf::Font font;
	sf::Text text;
	sf::CircleShape cs;

	myecs::MyEntityManager em;
	myecs::VerletIntegrator ms;
	myecs::RenderSystem rs;
	myecs::Logger lg;

public:
	int load(){

		// Create a graphical text to display
		if (!font.loadFromFile("arial.ttf"))
			return EXIT_FAILURE;
		text.setFont(font);
		text.setCharacterSize(30);


		// Create the entities
		myecs::makeEnemies(em, 10);



		return 0;
	}
	void move(float dt){
		ms.update(em, dt);
		lg.update(em, dt);
	}
	void render(sf::RenderWindow& window){
		rs.update(em, window);
		window.draw(text);

		cs.setPosition(400,300);
		cs.setOrigin(250,250);
		cs.setRadius(250);
		cs.setOutlineThickness(2);
		cs.setOutlineColor(sf::Color::White);
		cs.setFillColor(sf::Color::Transparent);
		cs.setPointCount(128);
		window.draw(cs);
	}
};




int main(){
	sf::ContextSettings settings(0,0,8);

	sf::RenderWindow window(sf::VideoMode(800, 600), "SFML window", sf::Style::Default, settings);

	Game game;
	game.load();

	FrameLimiter fl(120);
	fl.start();
	while (window.isOpen())
	{
		float dt = fl.getDeltaTime();

		// Process events
		sf::Event event;
		while (window.pollEvent(event))
		{
			// Close window: exit
			if (event.type == sf::Event::Closed)
				window.close();
		}
		// Clear screen
		window.clear();

		game.render(window);
		game.move(dt);

		// Update the window
		window.display();

	}
	return EXIT_SUCCESS;
}