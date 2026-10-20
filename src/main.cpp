#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

// Dynamic: click buffer.
// If you press the jump button a few frames before you could actually use it
// (landing on the ground, touching an orb), the press is remembered and applied
// the moment it becomes usable, as long as that happens within the buffer window.

namespace {
	bool enabled() { return Mod::get()->getSettingValue<bool>("enabled"); }
	bool bufferOrbs() { return Mod::get()->getSettingValue<bool>("buffer-orbs"); }
	bool bufferGround() { return Mod::get()->getSettingValue<bool>("buffer-ground"); }
	// Window length in seconds. "Frames" are frames of a 60 FPS recording.
	double windowSeconds() {
		return static_cast<double>(Mod::get()->getSettingValue<int64_t>("buffer-frames")) / 60.0;
	}

	bool groundBufferable(PlayerObject* p) {
		// Cube, ball, robot and spider jump from the ground. Ship, UFO, wave and swing do not.
		return !(p->m_isShip || p->m_isBird || p->m_isDart || p->m_isSwing);
	}
}

class $modify(DynBuffer, GJBaseGameLayer) {
	struct Pending {
		bool active = false;        // a press is waiting to be used
		double time = 0.0;          // level time of the press
		bool realHeld = false;      // is the physical button held down right now?
		bool injectedHeld = false;  // did we send a press that still needs releasing?
		double injectTime = 0.0;
	};

	struct Fields {
		Pending p[2];
	};

	bool isActivePlay() {
		auto pl = PlayLayer::get();
		return pl && static_cast<GJBaseGameLayer*>(pl) == this;
	}

	void handleButton(bool down, int button, bool isPlayer1) {
		if (button != 1 || !enabled() || !this->isActivePlay()) {
			GJBaseGameLayer::handleButton(down, button, isPlayer1);
			return;
		}

		int i = isPlayer1 ? 0 : 1;
		auto player = isPlayer1 ? m_player1 : m_player2;
		auto& p = m_fields->p[i];

		if (down) {
			p.realHeld = true;
			// If the press can't be used right now, remember it.
			if (player && !player->m_isDead && !player->m_isOnGround && player->m_touchedRings.empty()) {
				p.active = true;
				p.time = m_gameState.m_levelTime;
			} else {
				p.active = false;
			}
		} else {
			p.realHeld = false;
			p.injectedHeld = false;
		}

		GJBaseGameLayer::handleButton(down, button, isPlayer1);
	}

	void processCommands(float dt, bool isHalfTick, bool isLastTick) {
		if (enabled() && this->isActivePlay()) {
			this->bufferStep();
		}
		GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
	}

	void bufferStep() {
		double now = m_gameState.m_levelTime;
		double window = windowSeconds();

		for (int i = 0; i < 2; ++i) {
			bool isP1 = (i == 0);
			auto player = isP1 ? m_player1 : m_player2;
			if (!player) continue;
			auto& p = m_fields->p[i];

			// Release a press we injected once the real button is no longer held.
			if (p.injectedHeld && !p.realHeld && now > p.injectTime) {
				GJBaseGameLayer::handleButton(false, 1, isP1);
				p.injectedHeld = false;
			}

			if (!p.active) continue;

			// now < p.time means the level was restarted since the press.
			if (player->m_isDead || now < p.time || now - p.time > window) {
				p.active = false;
				continue;
			}

			bool onRing = bufferOrbs() && !player->m_touchedRings.empty();
			bool onGround = bufferGround() && player->m_isOnGround && groundBufferable(player);

			if (onRing) {
				GJBaseGameLayer::handleButton(true, 1, isP1);
				p.injectedHeld = true;
				p.injectTime = now;
				p.active = false;
			} else if (onGround) {
				// Still holding: the game jumps on landing by itself. Otherwise send the press.
				if (!p.realHeld) {
					GJBaseGameLayer::handleButton(true, 1, isP1);
					p.injectedHeld = true;
					p.injectTime = now;
				}
				p.active = false;
			}
		}
	}
};
