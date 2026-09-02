#pragma once

#include "DataHandler.hpp"

#define FRAME_DELAY() std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(TimeUtils::GetFrameDelta()))

class TimeUtils
{
public:

	enum CallResult
	{
		kLoopRepeat,
		kLoopTimeout,
		kEndDone,
		kEndTimedOut
	};

	template <typename TDuration, typename TCallback>
	static void WaitAndCall(TDuration duration, TCallback&& callback, const bool pausable = true)
	{
		constexpr auto tickInterval = std::chrono::duration_cast<std::chrono::nanoseconds>(100ms);

		auto totalDuration = [&]() {
			if constexpr (std::is_invocable_v<TDuration>) return duration();
			else return duration;
		}();

		auto remaining = std::make_shared<std::chrono::nanoseconds>(totalDuration);

		TimeUtils::DoWhile(
			[remaining, tickInterval]() {
				return std::max(std::min(tickInterval, *remaining), 0ns);
			},
			[remaining, callback = std::forward<TCallback>(callback)](CallResult result, std::chrono::nanoseconds delta) {
				if (IsEnd(result)) return callback(result, delta);

				*remaining -= tickInterval - delta;
				const auto timeResult = *remaining > 0ns;

				if (timeResult) return callback(result, delta);
				return false;
			},
			pausable
		);
	}

	template <typename TInterval, typename TCallback>
	static void DoWhile(TInterval interval, TCallback&& callback, const bool pausable = true)
	{
		constexpr auto maxAllowedDelta = 500ms;
		const auto currentLoadPoint = ModData::lastLoadPoint;

		std::jthread([=, callback = std::forward<TCallback>(callback)]() mutable {
			auto result = std::make_shared<std::atomic<CallResult>>(CallResult::kLoopRepeat);

			std::mutex mtx;
			std::condition_variable cv;
			bool nextLoopReady = true;

			while (IsLooping(*result)) {
				auto currentInterval = [interval]() {
					if constexpr (std::is_invocable_v<TInterval>) return interval();
					else return interval;
				}();

				auto lastTickTime = std::chrono::steady_clock::now();
				auto nextTickTime = lastTickTime + currentInterval;

				if (pausable) WaitForGameReady(true);

				{
					std::unique_lock lock(mtx);
					nextLoopReady = false;
				}

				SKSE::GetTaskInterface()->AddTask([result, &mtx, &cv, &nextLoopReady, lastTickTime, currentLoadPoint, callback]() {
					{
						std::lock_guard lock(mtx);
						nextLoopReady = true;
					}
					cv.notify_one();

					if (!IsLooping(*result)) return;

					auto delta = std::chrono::steady_clock::now() - lastTickTime;
					if (delta < maxAllowedDelta) delta = 0ns;

					if (ModData::lastLoadPoint == currentLoadPoint) {
						const bool continueLoop = callback(CallResult::kLoopRepeat, delta);
						if (!continueLoop) *result = CallResult::kEndDone;
					} else {
						const bool continueTimeout = callback(CallResult::kLoopTimeout, delta);
						if (!continueTimeout) *result = CallResult::kEndTimedOut;
					}
				});

				{
					std::unique_lock lock(mtx);
					cv.wait(lock, [&] { return nextLoopReady; });
				}

				if (pausable) WaitForGameReady(true);

				const auto sleepDuration = nextTickTime - std::chrono::steady_clock::now();
				if (sleepDuration > FRAME_DELAY()) std::this_thread::sleep_for(sleepDuration);
				else std::this_thread::sleep_for(FRAME_DELAY());
			}

			SKSE::GetTaskInterface()->AddTask([result, callback]() {
				callback(*result, 0ns);
			});
		}).detach();
	}

	static constexpr bool IsLooping(CallResult result)
	{
		return result == CallResult::kLoopRepeat || result == CallResult::kLoopTimeout;
	}

	static constexpr bool IsEnd(CallResult result)
	{
		return result == CallResult::kEndDone || result == CallResult::kEndTimedOut;
	}

	static bool WaitForGameReady(const bool ignoreLoadingMenu = false)
	{
		bool wasPaused = false;

		while (true) {
			if (auto ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) {
				auto loadingMenu = ui->GetMenu("Loading Menu");
				if (ignoreLoadingMenu && ui->numPausesGame == 1 && loadingMenu && loadingMenu->OnStack()) break;

				std::this_thread::sleep_for(FRAME_DELAY());
				wasPaused = true;
				continue;
			}

			std::promise<void> p;
			auto f = p.get_future();

			SKSE::GetTaskInterface()->AddTask([&p]() { p.set_value(); });
        
			auto start = std::chrono::high_resolution_clock::now();
			f.get();

			if ((std::chrono::high_resolution_clock::now() - start) > 300ms) {
				wasPaused = true;
				continue;
			}

			break;
		}

		return wasPaused;
	}

	static bool IsGamePaused(const bool ignoreLoadingMenu = false)
	{
		if (auto ui = RE::UI::GetSingleton(); ui) {
			if (ui->GameIsPaused()) {
				if (ignoreLoadingMenu) {
					if (auto loadingMenu = ui->GetMenu("Loading Menu"); loadingMenu && loadingMenu->OnStack()) {
						return false;
					}
				}
				return true;
			}
		}
		return false;
	}

	static float GetFrameRate()
	{
		const float delta = GetFrameDelta();
		if (delta <= 0.0f) return 0.0f;
		
		return 1.0f / delta;
	}

	static float GetFrameDelta()
	{
		auto* bsTimer = RE::BSTimer::GetSingleton();
		if (!bsTimer) return 0.00694444f; // 144Hz

		return bsTimer->realTimeDelta;
	}

	static float GetTimeMultiplier()
	{
		auto* bsTimer = RE::BSTimer::GetSingleton();
		if (!bsTimer) return 1.0f;

		return bsTimer->QGlobalTimeMultiplier();
	}
};
