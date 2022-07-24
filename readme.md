An as-direct-as-possible implementation of a compile-time entity-component system (ECS). Work in progress.

Minimal example:

```c++
// Define components
struct A {};
struct B {};
struct C {};
EntityManager<A, B, C> em;

// Create 1000 entities, each with components A,B,C
em.createEntities<A,B,C>(1000, [&](int i, EntityHandle eh, A& a, B& b, C& c){

	});
            
// Iterate over entities with both A and C components
em.forAllComponents<A, C>([](A& a, C& c) {
			
	});
```

