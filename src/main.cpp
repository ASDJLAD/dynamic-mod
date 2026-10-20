#include <Geode/Geode.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/CCTouchDispatcher.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <ctime>
#include <fstream>

#include "Analyzer.hpp"
#include "Hud.hpp"

using namespace geode::prelude;
using dynamic::Analyzer;

namespace {
	size_t minClicks() {
		return static_cast<size_t>(Mod::get()->getSettingValue<int64_t>("min-clicks"));
	}
	double windowMs() {
		return static_cast<double>(Mod::get()->getSettingValue<int64_t>("input-window-ms"));
	}
}

// Real hardware input: keyboard (key repeats are ignored).
class $modify(DynKeyboard, CCKeyboardDispatcher) {
	bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double timestamp) {
		if (!repeat) Analyzer::get().rawInput();
		return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, timestamp);
	}
};

// Real hardware input: mouse / touch (type 0 = began, 2 = ended).
class $modify(DynTouch, CCTouchDispatcher) {
	void touches(CCSet* touches, CCEvent* event, unsigned int type) {
		if (type == 0 || type == 2) Analyzer::get().rawInput();
		CCTouchDispatcher::touches(touches, event, type);
	}
};

// In-game click handling and per-frame timing.
class $modify(DynGameLayer, GJBaseGameLayer) {
	void update(float dt) {
		if (PlayLayer::get() == reinterpret_cast<PlayLayer*>(this)) {
			Analyzer::get().beginFrame(windowMs());
		}
		GJBaseGameLayer::update(dt);
	}

	void handleButton(bool down, int button, bool isPlayer1) {
		auto pl = PlayLayer::get();
		if (pl && pl == reinterpret_cast<PlayLayer*>(this)) {
			bool skip = pl->m_isPracticeMode && Mod::get()->getSettingValue<bool>("ignore-practice");
			if (!skip) {
				Analyzer::get().onButton(down, button, m_gameState.m_currentProgress);
			}
		}
		GJBaseGameLayer::handleButton(down, button, isPlayer1);
	}
};

class $modify(DynPlayLayer, PlayLayer) {
	void setupHasCompleted() {
		PlayLayer::setupHasCompleted();
		Analyzer::get().reset();
		if (Mod::get()->getSettingValue<bool>("show-hud")) {
			auto hud = DynamicHud::create();
			hud->setID("dynamic-hud"_spr);
			this->addChild(hud, 1000);
		}
	}

	void resetLevel() {
		PlayLayer::resetLevel();
		Analyzer::get().reset();
	}

	void levelComplete() {
		PlayLayer::levelComplete();

		if (m_isPracticeMode && Mod::get()->getSettingValue<bool>("ignore-practice")) return;
		if (Analyzer::get().clickCount() == 0) return;

		std::string name = m_level ? std::string(m_level->m_levelName) : std::string("Unknown");
		std::string text = Analyzer::get().report(minClicks(), name);

		if (Mod::get()->getSettingValue<bool>("save-reports")) {
			auto dir = Mod::get()->getSaveDir() / "reports";
			std::error_code ec;
			std::filesystem::create_directories(dir, ec);
			auto stamp = static_cast<long long>(std::time(nullptr));
			auto path = dir / fmt::format("run_{}.txt", stamp);
			std::ofstream out(path);
			if (out) out << text;
		}

		if (Mod::get()->getSettingValue<bool>("show-popup")) {
			FLAlertLayer::create("Dynamic", text, "OK")->show();
		}
	}
};
