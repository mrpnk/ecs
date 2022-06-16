#include <vector>
#include <tuple>
#include <array>
#include <bit>

namespace ecs_utils {

	// The first occurrence of type in a pack
	template<class T, class... U>
	constexpr size_t index = -1;

	template<class T, class... U>
	constexpr size_t index<T, T, U...> = 0;

	template<class T, class U0, class... U>
	constexpr size_t index<T, U0, U...> = 1 + index<T, U...>;


	template<class T>
	struct TypeIndex {
		using type = T;
		size_t i;
	};

	template<typename... Types>
	class TypeList {
		using tuple_t = std::tuple<Types...>;

	public:
		static constexpr void for_each(auto&& f) requires(std::is_invocable_v<decltype(f), TypeIndex<Types>>&& ...){
			[&]<std::size_t... i>(std::index_sequence<i...>) {
				(f(TypeIndex<std::tuple_element_t<i, tuple_t>>{i}), ...);
			}(std::make_index_sequence<sizeof...(Types)>{});
		}

		template<typename T>
		static constexpr std::size_t index_of() {
			return index<T, Types...>;
		}

		static constexpr size_t size() {
			return sizeof...(Types);
		}


		template<typename... A>
		class SubList {
			static constexpr size_t a[] = {index_of<A>()...};
		public:
			static constexpr bool in_order = std::is_sorted(std::cbegin(a), std::cend(a));
		};

		template<typename U>
		static constexpr bool is_any = (std::same_as<U, Types>||...);

		template<typename... U>
		static constexpr bool is_subset = (is_any<U>&&...);

		template<typename... U>
		static constexpr bool is_ordered_subset = is_subset<U...> && SubList<U...>::in_order;

	};


	template<typename U0, typename... U>
	constexpr bool is_duplicate_free = !(std::same_as<U0, U>||...) && is_duplicate_free<U...>;

	template<typename U0>
	constexpr bool is_duplicate_free<U0> = true;

}

namespace ecs
{
	using namespace ecs_utils;

	template<typename... TComponents> requires is_duplicate_free<TComponents...>
	class ComponentStorage{
		std::tuple<std::vector<TComponents>...> data;

	public:
		using TComponentBits = unsigned long long;// std::bitset<sizeof...(TComponents)>; bitset is not constexpr enough
		using TComponentList = TypeList<TComponents...>;

		// Returns the bit mask for the given combination of component types.
		template<class ...T> requires TComponentList::template is_subset<T...>
		static constexpr TComponentBits getMask() {
			static_assert(sizeof...(TComponents) <= sizeof(unsigned long long)*CHAR_BIT);
			unsigned long long b{0}; // workaround until std::bitset is more constexpr
			TypeList<T...>::for_each([&b](auto t)constexpr{
				b |= 1<<(TComponentList::template index_of<typename decltype(t)::type>());
			});
			return TComponentBits{b};
		}

		// Creates a new component of specified type.
		template<class TComponent> requires TComponentList::template is_any<TComponent>
		size_t createComponent(){
			constexpr auto ind = TComponentList::template index_of<TComponent>();
			auto& d = std::get<ind>(data);
			auto oldSize = d.size();
			d.resize(d.size() + 1);
			std::construct_at<TComponent>(d.data() + oldSize);
			return oldSize;
		}

		// Returns the pointer to the i-th component of specified type.
		template<class TComponent>
		TComponent* getData(size_t i){
			return std::get<TComponentList::template index_of<TComponent>()>(data).data() + i;
		}
	};


	template<typename... TComponents> requires is_duplicate_free<TComponents...>
	class EntityManager {
		using TComponentStorage = ComponentStorage<TComponents...>;
		using TComponentList = typename TComponentStorage::TComponentList;
		using TComponentBits = typename TComponentStorage::TComponentBits;

		struct Entity {
			TComponentBits bits;
			std::vector<std::size_t> compIndices;
		};

		std::vector<std::unique_ptr<Entity>> entities;
		TComponentStorage cs;

		// Returns the number of set bits in 'bits' to the right of the mask bit 'ask', given that bits&ask>0.
		static constexpr size_t getNumRight(TComponentBits const& bits, TComponentBits const& ask){
			return std::popcount(bits<<(std::countl_zero(ask)))-1;
		}

	public:
		// Creates 'num' new entities with the specified components.
		// For each new entity, calls 'initFunc' with the index [0,num) and references to components.
		template<class... TCreateComponents> requires TComponentList::template is_ordered_subset<TCreateComponents...>
		void createEntities(int num, auto&& initFunc) {
			constexpr auto sig = TComponentStorage::template getMask<TCreateComponents...>();
			using TL = TypeList<TCreateComponents...>;

			entities.reserve(entities.size() + num);
			for(int i = 0; i < num; ++i){
				auto e = std::make_unique<Entity>();
				e->bits = sig;
				e->compIndices.resize(sizeof...(TCreateComponents));

				TL::for_each([this, &e](auto t) {
					e->compIndices[t.i] = cs.template createComponent<typename decltype(t)::type>();
				});

				forAllComponents<TCreateComponents...>(e, initFunc, i);
				entities.push_back(std::move(e));
			}
		}

		// Calls 'f' once with passed 'args' and references to the specified components of the entity 'e'.
		template<class... TAskComponents, typename... Args>
		requires TComponentList::template is_ordered_subset<TAskComponents...>
		void forAllComponents(std::unique_ptr<Entity>& e, auto&& f, Args&&... args)
		requires std::is_invocable_v<decltype(f), Args..., TAskComponents&...>{
			const size_t idx[] = {getNumRight(e->bits, TComponentStorage::template getMask<TAskComponents>())...};
			[&, this]<size_t... i>(std::index_sequence<i...>){
				f(std::forward<Args>(args)...,
				  (*static_cast<TAskComponents*>(cs.template getData<TAskComponents>(e->compIndices[idx[i]])))...);
			}(std::make_index_sequence<sizeof...(TAskComponents)>{});
		}

		// Calls 'f' for all entities with references to the specified components.
		template<class... TAskComponents>
		void forAllComponents(auto&& f){
			static_assert(TComponentList::template is_ordered_subset<TAskComponents...>, "Component types must be an ordered subset.");
			constexpr TComponentBits sig = TComponentStorage::template getMask<TAskComponents...>();
			for(auto& e : entities)
				if(!(sig & ~e->bits)) //if((sig & ~e->bits).none())
					forAllComponents<TAskComponents...>(e, std::forward<decltype(f)>(f));
		}
	};

}

