#pragma once
#include <chrono>
#include <thread>

struct FrameLimiter {
	double targetFPS;
	double adjustment;
	std::chrono::high_resolution_clock hrc;
	decltype(hrc.now()) lastTime = hrc.now();
	FrameLimiter(float targetFPS) : targetFPS{targetFPS}{}
	void start() {
		lastTime = hrc.now();
		adjustment = 0;
	}
	float getDeltaTime()
	{
		using namespace std::chrono;
		auto currentTime = hrc.now();
		float dt = (float)duration_cast<nanoseconds>(currentTime - lastTime).count() / 1e9;
		std::this_thread::sleep_for(nanoseconds((long long)((1. / targetFPS - dt + adjustment) * 1e9)));
		std::this_thread::yield();
		currentTime = hrc.now();
		dt = (float)duration_cast<nanoseconds>(currentTime - lastTime).count() / 1e9;
		lastTime = currentTime;
		adjustment += (1. / targetFPS - dt) * 0.5;
		return dt;
	}
};
