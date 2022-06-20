#pragma once
#include <chrono>
#include <thread>

class FrameLimiter {
	double targetFPS;
	double adjustment;
	std::chrono::high_resolution_clock hrc;
	decltype(hrc.now()) lastTime = hrc.now();

	std::vector<float> frameTimes;
	float accFrameTime;
	int currentFrameTime;

public:
	FrameLimiter(float targetFPS, int nFrameTimes = 10) : targetFPS{targetFPS}{
		frameTimes.resize(nFrameTimes, 0);
	}

	// Should be called once before the first 'frame'.
	void start() {
		lastTime = hrc.now();
		adjustment = 0;
		accFrameTime = 0;
		currentFrameTime = 0;
	}

	// Signals the beginning of a new frame. Returns the used time since last frame.
	// 'dt' is set to the time, the game should be advanced with.
	float frame(){
		using namespace std::chrono;
		auto currentTime = hrc.now();
		float dt = (float)duration_cast<nanoseconds>(currentTime - lastTime).count() / 1e9;
		accFrameTime += dt - frameTimes[currentFrameTime];
		frameTimes[currentFrameTime] = dt;
		++currentFrameTime %= frameTimes.size();
		std::this_thread::sleep_for(nanoseconds((long long)((1. / targetFPS - dt + adjustment) * 1e9)));
		std::this_thread::yield();
		currentTime = hrc.now();
		dt = (float)duration_cast<nanoseconds>(currentTime - lastTime).count() / 1e9;
		lastTime = currentTime;
		adjustment += (1. / targetFPS - dt) * 0.5;

		return dt;
	}

	float getFrameTime(){
		return accFrameTime / (float)frameTimes.size();
	}

};
