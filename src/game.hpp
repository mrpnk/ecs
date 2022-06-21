#pragma once

#include "ecs.hpp"
#include "timer.hpp"
#include "colour.hpp"

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
	float restitution = 0.5; // 1 = elastic
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
	sf::Vector2f gravity = {0, 0.8};
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
			if(std::abs(ph.oldPos.x) < 0.05)
				accelerate(ph, -3.f*world.gravity);
			//accelerate(ph, (ph.oldPos.x<0?-1.f:1.f)*sf::Vector2f{ph.oldPos.y, -ph.oldPos.x} * 1.5f);
		});
	}
	void applyConstraint(MyEntityManager& em, float dt){
		em.forAllComponents<transform, physics>([this, dt](transform& tr, physics& ph) {
			auto conn = tr.pos - world.bowlCentre;
			auto dist = length(conn);
			if(dist > world.bowlRadius - ph.radius){
				sf::Vector2f n = conn/dist;
				float vn = dot(ph.vel, n);
				auto vt = ph.vel - vn*n;
				auto vt2 = lengthsq(vt);

				auto oldPos = tr.pos;
				tr.pos = world.bowlCentre + n*(world.bowlRadius - ph.radius);
				ph.vel = vt - vn*n * ph.restitution ;

				float absv2 = lengthsq(ph.vel);
				if(absv2 > 1e-6) {
					// conserve energy
					float ekin0 = vt2 + vn*vn * ph.restitution;
					float e0 = -dot(world.gravity, oldPos) + ekin0 / 2.f;
					float e1 = e0;
					float v2 = 2 * (e1 + dot(world.gravity, tr.pos));
					v2 = std::abs(v2); // might be negative when restitution is small
					ph.vel *= std::sqrt(v2 / absv2);
				}

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
//		em.forAllComponents<render>([this](render& re){
//			//	std::cout << "render address " << &re << std::endl;
//		});
	}
};


// The application class.
class Game{

	sf::Font font;
	sf::Text text;
	sf::CircleShape cs;

	MyEntityManager em;
	MotionSolver solver;
	Renderer renderer;
	Logger logger;

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
		const int num = 100;
		em.createEntities<struct transform,struct physics,struct render>(num,
            [&](int i, ecs::EntityHandle eh, struct transform& tr,struct physics& ph, struct render& re){
                ph.oldPos = tr.pos = world.bowlCentre+
						world.bowlRadius*sf::Vector2f{((float)i/(num-1)-0.5f)*2.f*0.9f, -0.5};

				re.radius = ph.radius = urd(mt);
                unsigned char hue = (float)i/num*255;
                auto rgb = HsvToRgb({hue,150,255});
                re.colour = sf::Color(rgb.r,rgb.g,rgb.b);
	            ph.restitution = 0.9;
            });


//
//		std::vector<sf::Vector2f> positions = { {-0.9,0}, {-1,-0.3}, {0.7,0}, {0,-0.3},{-0.01,-0.3} };
//		std::vector<sf::Color> colours = {sf::Color::Red, sf::Color::Blue, sf::Color::Green, sf::Color::Magenta, sf::Color::White};
//		int n = positions.size();
//		em.createEntities<struct transform,struct physics,struct render>(n,
//		[&](int i, ecs::EntityHandle eh, struct transform& tr,struct physics& ph, struct render& re){
//
//			ph.oldPos = tr.pos = positions[i] * world.bowlRadius;
//			re.radius = ph.radius = 0.01;
//			re.colour = colours[i%n];
//			ph.restitution = 0.9;
//		});


//		// conventional method
//		balls.resize(5000);
//		std::generate(balls.begin(),balls.end(),
//					  [&,i=0]()mutable{
//			ball ba;
//			ba.ph.oldPos = ba.tr.pos = world.bowlCentre+
//			                       world.bowlRadius*sf::Vector2f{((float)i/(num-1)-0.5f)*2.f*0.8f, -0.5};
//			ba.re.radius = ba.ph.radius =  urd(mt);
//			unsigned char hue = i*0.2;
//			auto rgb = HsvToRgb({hue,150,255});
//			ba.re.colour = sf::Color(rgb.r,rgb.g,rgb.b);
//			++i;
//			return ba;
//		});


//		ecs::EntityHandle handle;
//		em.createEntities<struct transform,struct render>(1,
//		  [&](int i, ecs::EntityHandle eh, struct transform& tr, struct render& re){
//            handle = eh;
//         });
//
//		em.attachComponents<struct physics>(handle,
//		[&](struct physics& ph){
//
//		});


		return 0;
	}
	void move(float dt){
		solver.update(em, dt);
		logger.update(em, dt);
	}
	void render(sf::RenderWindow& window, float frameTime){
		AutoTimer at(g_timer, _FUNC_);

		// Draw all balls
		renderer.update(em, window);

		{
			AutoTimer at(g_timer, "conv render");
			std::for_each(std::execution::par, balls.begin(), balls.end(),
						  [this, &window](ball& ba) {
				renderer.draw(window, ba.tr, ba.re);
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
		text.setString(std::to_string(logger.getEnergy()));
		text.setPosition(0,0.46);
		text.setOrigin(text.getLocalBounds().getSize()/2.f);
		window.draw(text);

		// Display frame time (unlimited)
		text.setString(std::to_string(frameTime));
		text.setPosition(-0.48,-0.46);
		window.draw(text);
	}
};

