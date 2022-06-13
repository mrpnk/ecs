#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include "ecs.hpp"
#include <chrono>

class Game{

	sf::Texture texture;
	sf::Font font;
	sf::Text text;


	myecs::MyEntityManager em;
	myecs::MoveSystem ms;
	myecs::RenderSystem rs;

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
	}
	void render(sf::RenderWindow& window){
		rs.update(em, window);
		window.draw(text);
	}
};




int main(){
	sf::RenderWindow window(sf::VideoMode(800, 600), "SFML window");

	Game game;
	game.load();

	auto t0 = std::chrono::high_resolution_clock::now(), t1=t0;
	while (window.isOpen())
	{
		t1 = std::chrono::high_resolution_clock::now();
		auto dt = std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count()/1.e6;
		t0 = t1;

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