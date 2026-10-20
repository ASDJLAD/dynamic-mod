#pragma once

#include <Geode/Geode.hpp>
#include "Analyzer.hpp"

// Small live readout in the top-left corner of the PlayLayer.
class DynamicHud : public cocos2d::CCNode {
public:
	static DynamicHud* create() {
		auto ret = new DynamicHud();
		if (ret && ret->init()) {
			ret->autorelease();
			return ret;
		}
		CC_SAFE_DELETE(ret);
		return nullptr;
	}

protected:
	cocos2d::CCLabelBMFont* m_label = nullptr;

	bool init() override {
		if (!CCNode::init()) return false;
		auto winSize = cocos2d::CCDirector::get()->getWinSize();
		m_label = cocos2d::CCLabelBMFont::create("Dynamic", "chatFont.fnt");
		m_label->setAnchorPoint({0.f, 1.f});
		m_label->setScale(0.5f);
		m_label->setOpacity(160);
		m_label->setPosition({6.f, winSize.height - 6.f});
		this->addChild(m_label);
		this->scheduleUpdate();
		return true;
	}

	void update(float) override {
		size_t minClicks = static_cast<size_t>(
			geode::Mod::get()->getSettingValue<int64_t>("min-clicks"));
		m_label->setString(dynamic::Analyzer::get().hudText(minClicks).c_str());
	}
};
