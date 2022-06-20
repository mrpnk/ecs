#include <SFML/Graphics.hpp>

#include "game.hpp"
#include "framerate.hpp"


int main(){
	sf::ContextSettings settings(0,0,8); // 8x antialiasing

	sf::RenderWindow window(sf::VideoMode(1200, 1000), "Entity Component System - Test",
							sf::Style::Titlebar | sf::Style::Close, settings);

	sf::View view;
	view.setCenter(0, 0);
	view.setSize((float)window.getSize().x/window.getSize().y,1.f);
	window.setView(view);

	Game game;
	game.load();

	FrameLimiter fl(120);
	fl.start();
	while (window.isOpen()){
		float dt = fl.frame();

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

		game.move(dt);
		game.render(window, fl.getFrameTime());

		// Update the window
		window.display();

	}
	g_timer.print();

	return 0;
}