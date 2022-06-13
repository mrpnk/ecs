#include <iostream>
#include <vector>
#include <memory>
#include <bitset>
#include <map>
#include <typeindex>
#include <tuple>
#include <array>

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
		static constexpr void helper(Func&& f, std::index_sequence<Is...>){
			(f(Type<std::tuple_element_t<Is,tuple_t>>{}), ...);
		}

	public:
		template<typename Func>
		static constexpr void for_each(Func&& f){
			helper(f, std::make_index_sequence<sizeof...(Args)>());
		}

		template<typename T>
		static constexpr std::size_t indexOf(){
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

	template<class TComponentList>
	class ComponentStorage{
		std::array<std::vector<std::byte>, TComponentList::size()> data;

	public:
		template<class T0, class ...T>
		static constexpr componentBits getMask() {
			// is actually not constexpr (and cannot use fold) because bitset::operator | is not constexpr
			if constexpr (!sizeof...(T))
				return componentBits(1 << TComponentList::template indexOf<T0>());
			else
				return getMask<T0>() | getMask<T...>();
		}

		template<class TComponent>
		size_t createComponent(){
			auto ind = TComponentList::template indexOf<TComponent>();
			auto& d = data[ind];
			auto oldSize = d.size();
			d.resize(d.size() + sizeof(TComponent));
			std::byte* mempos = d.data() + oldSize;
			std::construct_at<TComponent>((TComponent*)mempos);
			return oldSize;
		}

		template<class TComponent>
		void* getData(size_t byte){
			return data[TComponentList::template indexOf<TComponent>()].data()+byte;
		}
	};


	template<class MyComponentList>
	class EntityManager {
		std::vector<std::unique_ptr<entity>> entities;
		using TComponentStorage = ComponentStorage<MyComponentList>;
		TComponentStorage cs;

	public:
		EntityManager(){
		}

		template<class... TComponents>
		void createEntities(int num, auto&& initFunc) {
			const auto sig = TComponentStorage::template getMask<TComponents...>();
			entities.reserve(entities.size() + num);
			for(int i = 0; i < num; ++i){
				auto e = std::make_unique<entity>();
				e->bits = sig;
				e->compIndices.reserve(sizeof...(TComponents));
				TypeList<TComponents...>::for_each([this, &e](auto t){
					e->compIndices.push_back(cs.template createComponent<typename decltype(t)::type>());
				});
				forAllComponents<TComponents...>(e, initFunc, i);
				entities.push_back(std::move(e));
			}
		}



		template<class... TComponents, typename ...Args>
		void forAllComponents(std::unique_ptr<entity>& e, auto&& f, Args...args){
			[&, this]<size_t ...i>(std::index_sequence<i...> is){
				f((args)...,(*static_cast<TComponents*>(cs.template getData<TComponents>(e->compIndices[i])))...);
			}(std::make_index_sequence<sizeof...(TComponents)>{});
		}

		template<class... TComponents>
		void forAllComponents(auto&& f){
			componentBits sig = TComponentStorage::template getMask<TComponents...>();
			for(auto& e : entities)
				if((sig & ~e->bits).none()) {
					forAllComponents<TComponents...>(e, f);
				}
		}
	};

}

namespace myecs{

	using namespace ecs;

	struct transform {
		float x, y;
	};
	struct physics {
		float velx, vely;
	};
	struct render {
		int radius = 10;
		sf::Color colour = sf::Color::Red;
	};
	struct collision {
		float radius;
		float mass;
	};

	using MyComponentList = TypeList<transform, physics, render, collision>;
	using MyEntityManager = EntityManager<MyComponentList>;

	void makeEnemies(MyEntityManager& em, int num) {
		em.createEntities<transform,physics,render,collision>(num,
			[](int i, transform& tr, physics& ph, render& re, collision& co){
			tr.y = 0; tr.x = 100*i;
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
				shape.setPosition(tr.x, tr.y);
				window.draw(shape);
			});
		}
	};


	class MoveSystem {
		sf::CircleShape shape;
	public:
		void update(MyEntityManager& em, float dt){
			em.forAllComponents<transform, physics>([this, dt](transform& tr, physics& ph) {
				tr.x += dt * ph.velx;
				tr.y += dt * ph.vely;

				ph.vely = std::min(ph.vely + dt * 59.81, 400.);
			});
		}
	};

}

