#include <SFML/Graphics.hpp>

#include "game.hpp"
#include "framerate.hpp"


int main(){
	sf::ContextSettings settings(0,0,8);

	sf::RenderWindow window(sf::VideoMode(1200, 1000), "Entity Component System - Test", sf::Style::Default, settings);

	sf::View view;
	view.setCenter(0, 0);
	view.setSize((float)window.getSize().x/window.getSize().y,1.f);
	window.setView(view);

	Game game;
	game.load();

	FrameLimiter fl(12000);
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
	g_timer.print();

	return 0;
}