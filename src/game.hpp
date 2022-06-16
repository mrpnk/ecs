#pragma once

#include "ecs.hpp"
#include "colour.hpp"
#include "timer.hpp"
#include <execution>
#include <random>

using namespace ecs;
float dot(sf::Vector2f const& a, sf::Vector2f const& b){
	return a.x*b.x+a.y*b.y;
}
float length(sf::Vector2f const& v){
	return std::sqrt(v.x*v.x+v.y*v.y);
}
float lengthsq(sf::Vector2f const& v){
	return v.x*v.x+v.y*v.y;
}

// Define some components:
struct transform {
	sf::Vector2f pos;
};
struct physics {
	float mass;
	float radius;
	sf::Vector2f vel, velim, oldVel;
	sf::Vector2f oldPos;
	sf::Vector2f acc, oldAcc;
};
struct render {
	float radius;
	sf::Color colour;
};

// For comparison with a conventional approach.
struct ball{
	transform tr;
	physics ph;
	render re;
};

// Specify once which components there are
using MyEntityManager = EntityManager<transform, physics, render>;

// Global world properties
struct {
	sf::Vector2f gravity = {0, 1.2};
	sf::Vector2f bowlCentre = {0,0};
	float bowlRadius = 0.4;
}world;


// Define some systems:
class Renderer {
	sf::CircleShape shape;

public:
	void update(MyEntityManager& em, sf::RenderWindow& window){

		AutoTimer at(g_timer, _FUNC_);
		em.forAllComponents<transform, render>([this, &window](transform& tr, render& re) {
			shape.setRadius(re.radius);
			shape.setOrigin(re.radius, re.radius);
			shape.setFillColor(re.colour);
			shape.setPosition(tr.pos);
			window.draw(shape);
		});
	}

	void draw(sf::RenderWindow& window, transform& tr, render& re){
		shape.setRadius(re.radius);
		shape.setOrigin(re.radius, re.radius);
		shape.setFillColor(re.colour);
		shape.setPosition(tr.pos);
		window.draw(shape);
	}
};

class MotionSolver {
	void updatePosition(transform& tr, physics& ph, float dt){
		ph.oldPos = tr.pos;
		ph.oldVel = ph.vel;

		// drift-kick-drift
		ph.velim = ph.vel + ph.oldAcc*(dt/2.f);
		tr.pos += ph.velim*dt;
		ph.vel = ph.velim + ph.acc*(dt/2.f);

		ph.oldAcc = ph.acc;
		ph.acc = {0,0};
	}
	void accelerate(physics& ph, sf::Vector2f acc){
		ph.acc += acc;
	}

	void updatePositions(MyEntityManager& em, float dt){
		em.forAllComponents<transform, physics>([this, dt](transform& tr, physics& ph) {
			updatePosition(tr, ph, dt);
		});
	}
	void applyGravity(MyEntityManager& em){
		em.forAllComponents<physics>([this](physics& ph) {
			accelerate(ph, world.gravity);
		});
	}
	void applyConstraint(MyEntityManager& em, float dt){
		em.forAllComponents<transform, physics>([this, dt](transform& tr, physics& ph) {
			auto conn = tr.pos - world.bowlCentre;
			auto dist = length(conn);
			if(dist > world.bowlRadius - ph.radius){
				sf::Vector2f n = conn/dist;
				tr.pos = world.bowlCentre + n*(world.bowlRadius - ph.radius);
				float vn = dot(ph.vel, n);
				auto vt = ph.vel-vn*n;
				ph.vel = vt - vn*n;

				// conserve energy
				float e0 = -dot(world.gravity,ph.oldPos)+dot(ph.oldVel,ph.oldVel)/2.f;

				float v = sqrt(2*(e0+dot(world.gravity,tr.pos)));
				ph.vel *= v/length(ph.vel);

				float e2 = -dot(world.gravity,tr.pos)+dot(ph.vel,ph.vel)/2.f;

//					if(abs(e2 - e0) > abs(std::min(e0,e2))*0.001)
//						int sdf=2143;
				// Determine contact point



			}
		});
	}

public:
	void update(MyEntityManager& em, float dt) {
		applyGravity(em);
		applyConstraint(em, dt);
		updatePositions(em, dt);
	}
};

class Logger{
	bool activated = false;
	float time = 0;
	float energy; // per mass
public:
	float getEnergy() const{return energy;}
	void update(MyEntityManager& em, float dt){
		time += dt;
		energy = 0;
		em.forAllComponents<transform, physics>([this](transform& tr, physics& ph){
			if(tr.pos.y > 600 && ! activated){
				activated = true;
				std::cout << "Hit the bottom at " << time <<std::endl;
			}
			//std::cout << "transform address " << &tr << std::endl;
			energy += -dot(world.gravity, tr.pos) + lengthsq(ph.vel) / 2.f;
		});
		em.forAllComponents<render>([this](render& re){
			//	std::cout << "render address " << &re << std::endl;
		});
	}
};


// The application class.
class Game{

	sf::Texture texture;
	sf::Font font;
	sf::Text text;
	sf::CircleShape cs;

	MyEntityManager em;
	MotionSolver ms;
	Renderer rs;
	Logger lg;

	std::vector<ball> balls;

public:
	int load(){

		// Create a graphical text to display
		if (!font.loadFromFile("arial.ttf"))
			return EXIT_FAILURE;
		text.setFont(font);
		text.setCharacterSize(30);
		text.setScale({0.001,0.001});

		std::uniform_real_distribution<float> urd(0.002,0.02);
		std::mt19937 mt;

		// Create the entities
		const int num = 1000;
		em.createEntities<struct transform,struct physics,struct render>(num,
            [&](int i, struct transform& tr,struct physics& ph, struct render& re){
                ph.oldPos = tr.pos = world.bowlCentre+world.bowlRadius*sf::Vector2f{((float)i/(num-1)-0.5f)*2.f*0.8f, -0.2};

				re.radius = ph.radius = urd(mt);
                unsigned char hue = i*0.2;
                auto rgb = HsvToRgb({hue,150,255});
                re.colour = sf::Color(rgb.r,rgb.g,rgb.b);
            });


//		// conventional method
//		balls.resize(1000);
//		std::generate(balls.begin(),balls.end(),
//					  [i=0]()mutable{
//			ball ba;
//			ba.ph.oldPos = ba.tr.pos = {300+0.2f*i, 200};
//			ba.re.radius = ba.ph.radius = 15.f;
//			unsigned char hue = i*0.2;
//			auto rgb = HsvToRgb({hue,150,255});
//			ba.re.colour = sf::Color(rgb.r,rgb.g,rgb.b);
//			++i;
//			return ba;
//		});



		return 0;
	}
	void move(float dt){
		ms.update(em, dt);
		lg.update(em, dt);
	}
	void render(sf::RenderWindow& window){
		AutoTimer at(g_timer, _FUNC_);

		// Draw all balls
		rs.update(em, window);

		{
			AutoTimer at(g_timer, "conv render");
			std::for_each(std::execution::par, balls.begin(), balls.end(),
						  [this, &window](ball& ba) {
				rs.draw(window, ba.tr, ba.re);
			});
		}

		// Draw bowl
		cs.setPosition(world.bowlCentre);
		cs.setOrigin(world.bowlRadius, world.bowlRadius);
		cs.setRadius(world.bowlRadius);
		cs.setOutlineThickness(0.01);
		cs.setOutlineColor(sf::Color::White);
		cs.setFillColor(sf::Color::Transparent);
		cs.setPointCount(128);
		window.draw(cs);


		// Draw some text
		text.setString(std::to_string(lg.getEnergy()));
		text.setPosition(0,0.46);
		text.setOrigin(text.getLocalBounds().getSize()/2.f);
		window.draw(text);
	}
};

