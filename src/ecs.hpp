#include <iostream>
#include <vector>
#include <memory>
#include <bitset>
#include <map>
#include <typeindex>
#include <tuple>
#include <array>
#include <cmath>

// The first occurence of type in a tuple
template <class T, class Tuple>
struct Index;
template <class T, class... Types>
struct Index<T, std::tuple<T, Types...>> {
	static const std::size_t value = 0;
};
template <class T, class U, class... Types>
struct Index<T, std::tuple<U, Types...>> {
	static const std::size_t value = 1 + Index<T, std::tuple<Types...>>::value;
};

namespace ecs {

	template<class T>
	struct Type{
		using type = T;
	};
	template<typename... Args>
	class TypeList{
		using tuple_t = std::tuple<Args...>;

		template<typename Func, std::size_t... Is>
		static constexpr void iterate(Func&& f, std::index_sequence<Is...>){
			(f(Type<std::tuple_element_t<Is,tuple_t>>{}), ...);
		}

	public:
		static constexpr void for_each(auto&& f){
			iterate(f, std::make_index_sequence<sizeof...(Args)>());
		}

		template<typename T>
		static constexpr std::size_t index_of(){
			return Index<T, tuple_t>::value;
		}

		static constexpr size_t size(){
			return sizeof...(Args);
		}
	};


	using componentBits = std::bitset<64>;
	struct entity {
		std::vector<std::size_t> compIndices;
		componentBits bits;
	};

	template<typename... TComponents>
	class ComponentStorage{
		using TComponentList = TypeList<TComponents...>;
		std::tuple<std::vector<TComponents>...> data;

	public:
		template<class T0, class ...T>
		static constexpr componentBits getMask() {
			// is actually not constexpr (and cannot use fold) because bitset::operator | is not constexpr
			if constexpr (!sizeof...(T))
				return componentBits(1 << TComponentList::template index_of<T0>());
			else
				return getMask<T0>() | getMask<T...>();
		}

		template<class TComponent>
		size_t createComponent(){
			constexpr auto ind = TComponentList::template index_of<TComponent>();
			auto& d = std::get<ind>(data);
			auto oldSize = d.size();
			d.resize(d.size() + 1);
			auto* mempos = d.data() + oldSize;
			std::construct_at<TComponent>(mempos);
			return oldSize;
		}

		// Returns the pointer to the
		template<class TComponent>
		TComponent* getData(size_t byte){
			return std::get<TComponentList::template index_of<TComponent>()>(data).data() + byte;
		}
	};


	template<typename... TComponents>
	class EntityManager {
		using TComponentStorage = ComponentStorage<TComponents...>;
		std::vector<std::unique_ptr<entity>> entities;
		TComponentStorage cs;

	public:
		EntityManager(){ }

		// Creates num new entities with the specified components.
		// For each new entity, calls initFunc with the index [0,num) and references to components.
		template<class... TCreateComponents>
		void createEntities(int num, auto&& initFunc) {
			const auto sig = TComponentStorage::template getMask<TCreateComponents...>();
			entities.reserve(entities.size() + num);
			for(int i = 0; i < num; ++i){
				auto e = std::make_unique<entity>();
				e->bits = sig;
				e->compIndices.reserve(sizeof...(TCreateComponents));
				TypeList<TCreateComponents...>::for_each([this, &e](auto t) {
					e->compIndices.push_back(cs.template createComponent<typename decltype(t)::type>());
				});
				forAllComponents<TCreateComponents...>(e, initFunc, i);
				entities.push_back(std::move(e));
			}
		}


		// Calls f once with references to the specified components of the given entity.
		template<class... TCreateComponents, typename ...Args>
		void forAllComponents(std::unique_ptr<entity>& e, auto&& f, Args...args){
			[&, this]<size_t ...i>(std::index_sequence<i...> is){
				f((args)...,(*static_cast<TCreateComponents*>(cs.template getData<TCreateComponents>(e->compIndices[i])))...);
			}(std::make_index_sequence<sizeof...(TCreateComponents)>{});
		}

		// Calls f for all entities with references to the specified components.
		template<class... TCreateComponents>
		void forAllComponents(auto&& f){
			componentBits sig = TComponentStorage::template getMask<TCreateComponents...>();
			for(auto& e : entities)
				if((sig & ~e->bits).none()) {
					forAllComponents<TCreateComponents...>(e, f);
				}
		}
	};

}



namespace myecs{

	using namespace ecs;

	struct transform {
		sf::Vector2f pos;
	};
	struct physics {
		float mass;
		float radius;
		sf::Vector2f vel, velim;
		sf::Vector2f oldPos;
		sf::Vector2f acc, oldAcc;
	};
	struct render {
		int radius;
		sf::Color colour = sf::Color::Red;
	};

	using MyEntityManager = EntityManager<transform, physics, render>;

	void makeEnemies(MyEntityManager& em, int num) {
		em.createEntities<transform,physics,render>(num,
			[](int i, transform& tr, physics& ph, render& re){
			ph.oldPos = tr.pos = {160+50.f*i, 300};
			re.radius = ph.radius = 15;
		});
	}


	class RenderSystem {
		sf::CircleShape shape;
	public:
		void update(MyEntityManager& em, sf::RenderWindow& window){
			em.forAllComponents<transform, render>([this, &window](transform& tr, render& re) {
				shape.setRadius(re.radius);
				shape.setOrigin(re.radius, re.radius);
				shape.setFillColor(re.colour);
				shape.setPosition(tr.pos);
				window.draw(shape);
			});
		}
	};


	float dot(sf::Vector2f const& a, sf::Vector2f const& b){
		return a.x*b.x+a.y*b.y;
	}
	float length(sf::Vector2f const& v){
		return std::sqrt(v.x*v.x+v.y*v.y);
	}

	class VerletIntegrator {
		sf::Vector2f gravity = {0, 100};

		sf::Vector2f bowlCentre = {400,300};
		float bowlRadius = 250;

		void updatePosition(transform& tr, physics& ph, float dt){
			ph.oldPos = tr.pos;

			ph.velim = ph.vel + ph.oldAcc*(dt/2.f);
			tr.pos += ph.velim*dt;
			ph.vel = ph.velim + ph.acc*(dt/2.f);

			ph.oldAcc = ph.acc;
			ph.acc = {0,0};
		}
		void accelerate(physics& ph, sf::Vector2f acc){
			ph.acc += acc;
		}
	public:
		void update(MyEntityManager& em, float dt) {
			applyGravity(em);
			applyConstraint(em, dt);
			updatePositions(em, dt);
		}

		void updatePositions(MyEntityManager& em, float dt){
			em.forAllComponents<transform, physics>([this, dt](transform& tr, physics& ph) {
				updatePosition(tr, ph, dt);
			});
		}

		void applyGravity(MyEntityManager& em){
			em.forAllComponents<physics>([this](physics& ph) {
				accelerate(ph, gravity);
			});
		}

		void applyConstraint(MyEntityManager& em, float dt){
			em.forAllComponents<transform, physics>([this, dt](transform& tr, physics& ph) {
				auto conn = tr.pos - bowlCentre;
				auto dist = length(conn);
				if(dist > bowlRadius - ph.radius){
					sf::Vector2f n = conn/dist;
					tr.pos = bowlCentre + n*(bowlRadius - ph.radius);
					float vn = dot(ph.vel, n);
					auto vt = ph.vel-vn*n;
					ph.vel = vt - vn*n;
				}
			});
		}


	};


	class Logger{
		bool activated = false;
		float time = 0;
	public:
		void update(MyEntityManager& em, float dt){
			time += dt;
			em.forAllComponents<transform>([this](transform& tr){
				if(tr.pos.y > 600 && ! activated){
					activated = true;
					std::cout << "Hit the bottom at " << time <<std::endl;
				}
			});
		}
	};
}

