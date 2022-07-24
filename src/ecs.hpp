#pragma once

#include <vector>
#include <tuple>
#include <array>
#include <bit>
#include <numeric>
#include <algorithm>

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
		static constexpr void for_each(auto&& f) requires(std::is_invocable_v<decltype(f), TypeIndex<Types>>&& ...) {
			[&] <std::size_t... i>(std::index_sequence<i...>) {
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
			static constexpr size_t a[] = { index_of<A>()... };
		public:
			static constexpr bool in_order = std::is_sorted(std::cbegin(a), std::cend(a));

			//			template<class... B>
			//			static constexpr auto get_order(){
			//				std::array<int, sizeof...(A)+sizeof...(B)> arr{index_of<A>()..., index_of<B>()...};
			//				//std::sort(std::begin(arr), std::end(arr)); // std::inplace_merge is not constexpr
			//				return arr;
			//			}
		};
		template<>
		class SubList<> {

		public:
			static constexpr bool in_order = true;
		};


		template<typename U>
		static constexpr bool is_any = (std::same_as<U, Types> || ...);

		template<typename... U>
		static constexpr bool is_subset = (is_any<U>&&...);

		template<typename... U>
		static constexpr bool is_ordered_subset = is_subset<U...> && SubList<U...>::in_order;

	};

	template <typename T>
	void inplace_permute2(
		std::vector<T>& vec,
		const std::vector<std::size_t>& p)
	{
		std::vector<bool> done(vec.size());
		for (std::size_t i = 0; i < vec.size(); ++i) {
			if (done[i])
				continue;

			done[i] = true;
			std::size_t prev_j = i;
			std::size_t j = p[i];
			while (i != j) {
				std::swap(vec[prev_j], vec[j]);
				done[j] = true;
				prev_j = j;
				j = p[j];
			}
		}
	}


	// Permutes 'a' in-place such that a[i] |-> a[p[i]].
	template<typename A, typename P>
	void inplace_permute(A&& a, P&& p) {
		for (size_t i = 0; i < a.size(); ++i) {
			size_t curr = i;
			size_t next = p[curr];
			while (next != i) {
				std::swap(a[curr], a[next]);
				p[curr] = curr;
				curr = next;
				next = p[next];
			}
			p[curr] = curr;
		}
	}



	template<typename U0, typename... U>
	constexpr bool is_duplicate_free = !(std::same_as<U0, U> || ...) && is_duplicate_free<U...>;

	template<typename U0>
	constexpr bool is_duplicate_free<U0> = true;
}

namespace ecs
{
	using namespace ecs_utils;

	// Class to store the data of components.
	template<typename... TComponents> requires is_duplicate_free<TComponents...>
	class ComponentStorage {
		std::tuple<std::vector<TComponents>...> data;

	public:
		using TComponentBits = unsigned long long;// std::bitset<sizeof...(TComponents)>; bitset is not constexpr enough
		using TComponentList = TypeList<TComponents...>;

		// Returns the bit mask for the given combination of component types.
		template<class... T> requires TComponentList::template is_subset<T...>
		static constexpr TComponentBits getMask() {
			static_assert(sizeof...(TComponents) <= sizeof(unsigned long long) * CHAR_BIT);
			unsigned long long b{ 0 }; // workaround until std::bitset is more constexpr
			TypeList<T...>::for_each([&b](auto t)constexpr {
				b |= 1 << (TComponentList::template index_of<typename decltype(t)::type>());
				});
			return TComponentBits{ b };
		}

		// Creates a new component of specified type.
		template<class TComponent, typename... Args> requires TComponentList::template is_any<TComponent>
		size_t createComponent(Args&&...args) {
			constexpr auto ind = TComponentList::template index_of<TComponent>();
			auto& d = std::get<ind>(data);
			auto oldSize = d.size();
			d.resize(d.size() + 1);
			std::construct_at<TComponent>(d.data() + oldSize, std::forward<Args>(args)...);
			return oldSize;
		}

		// Returns the pointer to the i-th component of specified type.
		template<class TComponent>
		TComponent* getData(size_t i) {
			return std::get<TComponentList::template index_of<TComponent>()>(data).data() + i;
		}
	};

	// Handle on a single entity.
	struct EntityHandle {
		size_t idx;
	};

	const EntityHandle emptyHandle = { -1 }; // TODO this will intentionally crash eventually

	// Class to store and manage entities.
	template<typename... TComponents> requires is_duplicate_free<TComponents...>
	class EntityManager {

		using TComponentStorage = ComponentStorage<TComponents...>;
		using TComponentList = typename TComponentStorage::TComponentList;
		using TComponentBits = typename TComponentStorage::TComponentBits;

		struct Entity {
			TComponentBits bits;
			std::vector<std::size_t> compIndices;
			bool isPrefab; // TODO more elegant would be if the prefab property is a component
		};

		std::vector<Entity> entities;
		TComponentStorage cs;

		// Returns the number of set bits in 'bits' to the right of the mask bit 'ask', given that bits&ask>0.
		static constexpr size_t getNumRight(TComponentBits const& bits, TComponentBits const& ask) {
			return std::popcount(bits << (std::countl_zero(ask))) - 1;
		}

		// Calls 'f' once with passed 'args' and references to the specified components of the entity 'e'.
		template<class... TAskComponents, typename... Args>
			requires TComponentList::template is_ordered_subset<TAskComponents...>
		void forAllComponents(Entity& e, auto&& f, Args&&... args) {
			const size_t idx[] = { getNumRight(e.bits, TComponentStorage::template getMask<TAskComponents>())... };
			[&, this] <size_t... i>(std::index_sequence<i...>) {
				f(std::forward<Args>(args)...,
					(*static_cast<TAskComponents*>(cs.template getData<TAskComponents>(e.compIndices[idx[i]])))...);
			}(std::make_index_sequence<sizeof...(TAskComponents)>{});
		}

		inline Entity& getEntity(EntityHandle const& eh) {
			return entities[eh.idx];
		}
		inline EntityHandle getHandle(Entity const& e) {
			return { static_cast<size_t>(&e - entities.data()) };
		}

		void fill(TComponentBits const& bits, auto&& a) {
			int n = std::popcount(bits);
			int k = 0;
			for (unsigned i = 0; i < sizeof(bits) * CHAR_BIT; ++i)
				if (bits & (1 << i)) {
					a[k++] = i;
					if (k == n) return;
				}
		}

		template<class... TCreateComponents> requires TComponentList::template is_ordered_subset<TCreateComponents...>
		void attachComponents(Entity& e) {
			constexpr auto sig = TComponentStorage::template getMask<TCreateComponents...>();
			using TL = TypeList<TCreateComponents...>;

			const auto n0 = e.compIndices.size();
			const auto n1 = n0 + sizeof...(TCreateComponents);
			e.compIndices.resize(n1);

			TL::for_each([this, &e, &n0](auto t) {
				e.compIndices[n0 + t.i] = cs.template createComponent<typename decltype(t)::type>();
				});


			// Sort the compIndices according to the components they refer to:
			std::vector<int> a(n1);
			fill(e.bits, a);
			fill(sig, a.data() + n0);

			std::vector<int> idx(n1);
			std::iota(idx.begin(), idx.end(), 0);
			std::sort(idx.begin(), idx.end(), [&a](int l, int r) {return a[l] < a[r]; });
			inplace_permute(e.compIndices, idx);

			e.bits |= sig;
		}


	public:
		// Creates 'num' new entities with the specified components.
		// For each new entity, calls 'initFunc' with the index [0,num) and references to components.
		template<class... TCreateComponents> requires TComponentList::template is_ordered_subset<TCreateComponents...>
		void createEntities(int num, auto&& initFunc) {
			static_assert(std::is_invocable_v<decltype(initFunc), size_t, EntityHandle, TCreateComponents&...>, "The callback for 'createEntities' needs to take (size_t, EntityHandle, TCreateComponents&...).");
			auto n0 = entities.size();
			entities.resize(n0 + num);
			for (size_t i = 0; i < num; ++i) {
				Entity& e = entities[n0 + i];
				e.isPrefab = prefabbing;
				attachComponents<TCreateComponents...>(e);
				forAllComponents<TCreateComponents...>(e, initFunc, i, EntityHandle{ n0 + i });
			}
		}

		// Calls 'f' for all entities with references to the specified components.
		template<class... TAskComponents>
		void forAllComponents(auto&& f) {
			static_assert(TComponentList::template is_ordered_subset<TAskComponents...>, "Component types must be an ordered subset.");
			static_assert(std::is_invocable_v<decltype(f), TAskComponents&...>, "The callback for 'forAllComponents' needs to take (TAskComponents&...).");
			constexpr TComponentBits ask = TComponentStorage::template getMask<TAskComponents...>();
			for (auto& e : entities)
				if (!(ask & ~e.bits) && !e.isPrefab)
					forAllComponents<TAskComponents...>(e, std::forward<decltype(f)>(f));
		}

		// Calls 'f' once with passed 'args' and references to the specified components of the entity handle 'handle'.
		template<class... TAskComponents, typename... Args>
		void forAllComponents(EntityHandle const& eh, auto&& f, Args&&... args) {
			static_assert(std::is_invocable_v<decltype(f), Args..., TAskComponents&...>, "The callback for 'forAllComponents' needs to take (Args&&..., TAskComponents&...).");
			forAllComponents<TAskComponents...>(getEntity(eh), std::forward<decltype(f)>(f), std::forward<decltype(args)>(args)...);
		}

		// Adds the specified components to the entity behind the handle. Calls 'initFunc' with the new components
		template<class... TCreateComponents> requires TComponentList::template is_ordered_subset<TCreateComponents...>
		void attachComponents(EntityHandle handle, auto&& initFunc) {
			auto& e = getEntity(handle);
			attachComponents<TCreateComponents...>(e);
			forAllComponents<TCreateComponents...>(e, initFunc);
		}

		// Adds a new entity whose components are copies of the componenets behind 'handle'.
		EntityHandle duplicateEntity(EntityHandle const& handle) {
			Entity& e = entities.emplace_back(getEntity(handle));
			e.isPrefab = prefabbing;

			size_t i = 0;
			TComponentList::for_each([this, &e, &i](auto t) {
				using T = typename decltype(t)::type;
				if (e.bits & cs.getMask<T>()) {
					auto old = *(cs.template getData<T>(e.compIndices[i])); // temporarily save the old object because createComponent resizes.
					e.compIndices[i++] = cs.template createComponent<T>(std::move(old));
				}
				});

			return getHandle(e);
		}

		bool prefabbing = true;
		void setPrefabbing(bool b) {
			prefabbing = b;
		}
	};

}
