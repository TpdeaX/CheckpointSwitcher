#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/UILayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include "Keybinds.hpp"
#include <stdexcept>
#include <optional>

using namespace geode::prelude;

#ifdef GEODE_IS_WINDOWS
namespace matdash {
	struct Console {
		std::ofstream out, in;
		Console() {
			AllocConsole();
			out = decltype(out)("CONOUT$", std::ios::out);
			in = decltype(in)("CONIN$", std::ios::in);
			std::cout.rdbuf(out.rdbuf());
			std::cin.rdbuf(in.rdbuf());

			FILE* dummy;
			freopen_s(&dummy, "CONOUT$", "w", stdout);
		}
		~Console() {
			out.close();
			in.close();
		}
	};

	inline void create_console() {
		static Console console;
	}
}
#endif

$execute{
	using namespace keybinds;

	BindManager::get()->registerBindable({
		"switch-left-checkpoint-switcher"_spr,
		"Left Key",
		"L key",
		{ Keybind::create(KEY_Left) },
		Category::PLAY
	});

	BindManager::get()->registerBindable({
		"switch-right-checkpoint-switcher"_spr,
		"Right Key",
		"W key",
		{ Keybind::create(KEY_Right) },
		Category::PLAY
		});
}

namespace Utils {
	cocos2d::CCSize winSize() { return CCDirector::get()->getWinSize(); }
	GLubyte convertOpacitySimplf(float opaTM) { return static_cast<GLubyte>(255 * opaTM); }
	template <class R, class T>
	R& from(T base, intptr_t offset) {
		return *reinterpret_cast<R*>(reinterpret_cast<uintptr_t>(base) + offset);
	}

	template<typename T>
	const char* get_type_name(T*) {
		return typeid(T).name();
	}

	float calculatePercentageWithFixedPosition(int fixedFrame, float fixedPosition) {
		if (!PlayLayer::get()) {
			return 0.f;
		}

		auto player = Utils::from<PlayerObject*>(PlayLayer::get(), 0x878);
		auto xPosition = fixedPosition;
		auto levelTotalLength = Utils::from<float>(PlayLayer::get(), 0x2aa0);

		auto gameLevel = Utils::from<GJGameLevel*>(PlayLayer::get(), 0x5e0);
		auto levelLengthInFrames = Utils::from<int>(gameLevel, 0x414);

		if (0.f < levelLengthInFrames) {
			xPosition = fixedFrame;
			levelTotalLength = levelLengthInFrames;
		}

		float currentPercentage = (xPosition / levelTotalLength) * 100.0;

		if (currentPercentage > 100.0) {
			currentPercentage = 100.f;
		}
		else if (currentPercentage < 0.0) {
			currentPercentage = 0.0;
		}

		return currentPercentage;
	}



	template<typename T>
	std::string tryConvert(void* ptr) {
		try {
			T value = *reinterpret_cast<T*>(ptr);
			return typeid(T).name();
		}
		catch (const std::exception&) {
			return "Ningún tipo de dato reconocido";
		}
	}

	template <typename T>
	T convert_from_void(void* ptr) {
		try {
			return *reinterpret_cast<T*>(ptr);
		}
		catch (const std::bad_cast&) {
			return T();
		}
	}

	std::string from_void_string(void* ptr) {
		try {
			// Cast to char* to treat the memory as a character array
			char* char_ptr = reinterpret_cast<char*>(ptr);

			size_t max_length = 1024;
			size_t length = 0;
			while (length < max_length && char_ptr[length] != '\0') {
				length++;
			}

			if (length < max_length) {
				return std::string(char_ptr, length);
			}
			else {
				throw std::runtime_error("Null terminator not found within limit");
			}
		}
		catch (...) {
			return "";
		}
	}



	const char* getNameObj(cocos2d::CCNode* obj) {
		return (typeid(*obj).name() + 6);
	}

	template<typename T>
	struct IsPointerType {
		static constexpr bool value = false;
	};

	template<typename T>
	struct IsPointerType<T*> {
		static constexpr bool value = true;
	};

	template<typename T>
	void checkPrimitiveType(void* ptr, const T& expectedType) {
		if (!IsPointerType<T>::value) {
			throw std::invalid_argument("El tipo especificado no es un puntero.");
		}

		if (typeid(*reinterpret_cast<typename std::remove_pointer<T>::type*>(ptr)) != typeid(expectedType)) {
			std::cerr << "Error: El tipo de dato no coincide con el esperado." << std::endl;
		}
		else {
			std::cout << "El tipo de dato coincide con el esperado." << std::endl;
		}
	}

	std::string interpretMemoryType(void* ptr) {
		try {
			checkPrimitiveType(ptr, typeid(bool));
			return "Es un bool";
		}
		catch (const std::exception&) {
			try {
				checkPrimitiveType(ptr, typeid(int));
				return "Es un entero";
			}
			catch (const std::exception&) {
				try {
					checkPrimitiveType(ptr, typeid(float));
					return "Es un flotante";
				}
				catch (const std::exception&) {
					try {
						checkPrimitiveType(ptr, typeid(std::string));
						return "Es un std::string";
					}
					catch (const std::exception&) {
						return "Tipo de dato no compatible";
					}
				}
			}
		}
	}
}

class CheckpointSwitcher : public cocos2d::CCNode {
public:
	int index = 0;
	int m_maxCheckpointNow = 0;
	cocos2d::CCMenu* m_pMenuMain = nullptr;
	cocos2d::CCLabelBMFont* m_plabelLeft = nullptr;
	cocos2d::CCLabelBMFont* m_plabelMedium = nullptr;
	cocos2d::CCLabelBMFont* m_plabelRight = nullptr;
	CCMenuItemSpriteExtra* m_pleftArrowButton = nullptr;
	CCMenuItemSpriteExtra* m_prightArrowButton = nullptr;

	static CheckpointSwitcher* create();
	bool init();
	void switchCheckpoint(int i);
	void updateCheckpoints();
	void onLeftCheckpoint(cocos2d::CCObject* pSender);
	void onRightCheckpoint(cocos2d::CCObject* pSender);
	void updateUI();
	void onFadeInterface();
};

CheckpointSwitcher* CheckpointSwitcher::create() {
	auto node = new CheckpointSwitcher;
	if (node && node->init()) {
		node->autorelease();
	}
	else {
		CC_SAFE_DELETE(node);
	}
	return node;
}

bool CheckpointSwitcher::init() {
	if (!cocos2d::CCNode::init()) {
		return false;
	}

	auto playLayer = PlayLayer::get();
	if (playLayer) {
		auto ui = playLayer->m_uiLayer;

		this->m_pMenuMain = cocos2d::CCMenu::create();
		ui->addChild(this->m_pMenuMain);

		const auto WinSize = Utils::winSize();
		const auto LabelOpacity = 255.f;
		const auto LabelFont = "bigFont.fnt";
		const auto ArrowScale = 0.5f;
		const auto ArrowRotation = 180.f;
		const auto ArrowOffsetX = 50.f;
		const auto LabelOffsetY = 15.f - this->m_pMenuMain->getPositionY();
		const auto LabelCenter = ccp((WinSize.width / 2.f) - this->m_pMenuMain->getPositionX(), (WinSize.height / 2.f) - this->m_pMenuMain->getPositionY());

		m_plabelLeft = cocos2d::CCLabelBMFont::create("0/0", LabelFont);
		m_plabelLeft->setScale(ArrowScale);
		m_plabelLeft->setPosition(ccp(LabelCenter.x - ArrowOffsetX - 20.f, LabelOffsetY));
		m_plabelLeft->setAnchorPoint({ 1.f, 0.5f });
		m_plabelLeft->setOpacity(LabelOpacity);

		m_plabelMedium = cocos2d::CCLabelBMFont::create("0/0", LabelFont);
		m_plabelMedium->setScale(ArrowScale);
		m_plabelMedium->setPosition(ccp(LabelCenter.x, LabelOffsetY));
		m_plabelMedium->setOpacity(LabelOpacity);

		m_plabelRight = cocos2d::CCLabelBMFont::create("0/0", LabelFont);
		m_plabelRight->setScale(ArrowScale);
		m_plabelRight->setPosition(ccp(LabelCenter.x + ArrowOffsetX + 20.f, LabelOffsetY));
		m_plabelRight->setAnchorPoint({ 0.f, 0.5f });
		m_plabelRight->setOpacity(LabelOpacity);

		auto leftArrow = cocos2d::CCSprite::createWithSpriteFrameName("GJ_arrow_02_001.png");
		auto rightArrow = cocos2d::CCSprite::createWithSpriteFrameName("GJ_arrow_02_001.png");
		leftArrow->setScale(ArrowScale);
		rightArrow->setScale(ArrowScale);
		m_pleftArrowButton = CCMenuItemSpriteExtra::create(leftArrow, this, (cocos2d::SEL_MenuHandler)(&CheckpointSwitcher::onLeftCheckpoint));
		m_pleftArrowButton->setPosition(ccp(LabelCenter.x - ArrowOffsetX, LabelOffsetY));
		m_prightArrowButton = CCMenuItemSpriteExtra::create(rightArrow, this, (cocos2d::SEL_MenuHandler)(&CheckpointSwitcher::onRightCheckpoint));
		m_prightArrowButton->setRotationY(ArrowRotation);
		m_prightArrowButton->setPosition(ccp(LabelCenter.x + ArrowOffsetX, LabelOffsetY));
		m_pleftArrowButton->setOpacity(LabelOpacity);
		m_prightArrowButton->setOpacity(LabelOpacity);

		this->m_pMenuMain->addChild(m_pleftArrowButton);
		this->m_pMenuMain->addChild(m_prightArrowButton);
		this->m_pMenuMain->addChild(m_plabelLeft);
		this->m_pMenuMain->addChild(m_plabelMedium);
		this->m_pMenuMain->addChild(m_plabelRight);

		if (!GameManager::get()->getGameVariable("0024")) {
			this->m_pMenuMain->setEnabled(false);
		}

	}

	return true;
}

void CheckpointSwitcher::updateUI() {
	auto playLayer = PlayLayer::get();

	if (!playLayer)return;

	m_pMenuMain->setVisible(playLayer->m_isPracticeMode &&
		Mod::get()->getSettingValue<bool>("enable-mod") &&
		!Mod::get()->getSettingValue<bool>("disable-interface-switch"));

	if (!Mod::get()->getSettingValue<bool>("enable-mod"))return;

	auto getIndex = [this](int i) -> int {
		int index;
		if (i > static_cast<int>(this->m_maxCheckpointNow - 1)) {
			index = -1;
		}
		else if (i < -1) {
			index = static_cast<int>(this->m_maxCheckpointNow - 1);
		}
		else {
			index = i;
		}
		return index;
		};

	this->m_plabelMedium->limitLabelWidth(70.f, 0.5f, 0.1f);

	const auto ArrowOffsetX = 50.f;
	const auto LabelOffsetY = 15.f - this->m_pMenuMain->getPositionY();
	const auto WinSize = Utils::winSize();
	const auto LabelCenter = ccp((WinSize.width / 2.f) - this->m_pMenuMain->getPositionX(), (WinSize.height / 2.f) - this->m_pMenuMain->getPositionY());

	m_plabelLeft->setPosition(ccp(LabelCenter.x - ArrowOffsetX - 20.f, LabelOffsetY));
	m_plabelMedium->setPosition(ccp(LabelCenter.x, LabelOffsetY));
	m_plabelRight->setPosition(ccp(LabelCenter.x + ArrowOffsetX + 20.f, LabelOffsetY));
	m_pleftArrowButton->setPosition(ccp(LabelCenter.x - ArrowOffsetX, LabelOffsetY));
	m_prightArrowButton->setPosition(ccp(LabelCenter.x + ArrowOffsetX, LabelOffsetY));
	/*
	auto switcherStartPosNode = reinterpret_cast<StartPosSwitcher*>(NewFuncHooks::PlayLayer_getStartPosSwitcherM(Utils::getplayLayerA()));

	if (switcherStartPosNode && switcherStartPosNode->m_pleftArrowButton->isVisible() && switcherStartPosNode->m_pleftArrowButton->getOpacity() != 0) {

		m_plabelLeft->setPositionY(switcherStartPosNode->m_plabelLeft->getPositionY() + 20.f);
		m_plabelMedium->setPositionY(switcherStartPosNode->m_plabelMedium->getPositionY() + 20.f);
		m_plabelRight->setPositionY(switcherStartPosNode->m_plabelRight->getPositionY() + 20.f);
		m_pleftArrowButton->setPositionY(switcherStartPosNode->m_pleftArrowButton->getPositionY() + 20.f);
		m_prightArrowButton->setPositionY(switcherStartPosNode->m_prightArrowButton->getPositionY() + 20.f);
	}
	else if (sharedStateA().a_ProgressBarDown && (playLayer->m_sliderBarSprite->isVisible() ||
		reinterpret_cast<cocos2d::CCNode*>(playLayer->m_percentLabel->isVisible()))) {

		auto ySum = playLayer->m_percentLabel->getPositionY() + 5.f;

		m_plabelLeft->setPositionY(m_plabelLeft->getPositionY() + ySum);
		m_plabelMedium->setPositionY(m_plabelMedium->getPositionY() + ySum);
		m_plabelRight->setPositionY(m_plabelRight->getPositionY() + ySum);
		m_pleftArrowButton->setPositionY(m_pleftArrowButton->getPositionY() + ySum);
		m_prightArrowButton->setPositionY(m_prightArrowButton->getPositionY() + ySum);
	}
	else */
	if (playLayer->m_uiLayer->getChildByID("checkpoint-menu")->isVisible()) {
		auto ref = reinterpret_cast<cocos2d::CCNode*>(playLayer->m_uiLayer->getChildByID("checkpoint-menu")->getChildren()->objectAtIndex(0));
		auto ref2 = reinterpret_cast<cocos2d::CCNode*>(playLayer->m_uiLayer->getChildByID("checkpoint-menu")->getChildren()->objectAtIndex(1));

		auto maxScaleContentSize = std::max(ref->getScaledContentSize().height, ref2->getScaledContentSize().height);

		m_plabelLeft->setPositionY(m_plabelLeft->getPositionY() + maxScaleContentSize + 20.f);
		m_plabelMedium->setPositionY(m_plabelMedium->getPositionY() + maxScaleContentSize + 20.f);
		m_plabelRight->setPositionY(m_plabelRight->getPositionY() + maxScaleContentSize + 20.f);
		m_pleftArrowButton->setPositionY(m_pleftArrowButton->getPositionY() + maxScaleContentSize + 20.f);
		m_prightArrowButton->setPositionY(m_prightArrowButton->getPositionY() + maxScaleContentSize + 20.f);
	}

	this->m_plabelLeft->setVisible(Mod::get()->getSettingValue<bool>("show-percentage-mod") && !PlayLayer::get()->m_level->isPlatformer());
	this->m_plabelRight->setVisible(Mod::get()->getSettingValue<bool>("show-percentage-mod") && !PlayLayer::get()->m_level->isPlatformer());


	/*
	checkpoint = 0x2e18
	startpos = 0x2a40
	
	*/

	auto getFrameCheckpoint = [&](int i) -> int {
		if (i == -1) {
			auto startPos = Utils::from<StartPosObject*>(playLayer, 0x2a40);
			return startPos ? startPos->getPositionX() : 0;
		}
		else {
			auto checkpointArray = Utils::from<CCArray*>(playLayer, 0x2e18);
			if (checkpointArray) {
				auto checkpointObject = reinterpret_cast<CheckpointObject*>(checkpointArray->objectAtIndex(i));
				return checkpointObject ? Utils::from<GJGameState>(checkpointObject, 0x108).m_unk1f8 : 0;
			}
		}
		return 0; 
	};

	auto getPositionCheckpoint = [&](int i) -> float {
		if (i == -1) {
			auto startPos = Utils::from<StartPosObject*>(playLayer, 0x2a40);
			return startPos ? startPos->getPositionX() : 0;
		}
		else {
			auto checkpointArray = Utils::from<CCArray*>(playLayer, 0x2e18);
			if (checkpointArray) {
				auto checkpointObject = reinterpret_cast<CheckpointObject*>(checkpointArray->objectAtIndex(i));
				if (checkpointObject) {
					auto gameObject = Utils::from<GameObject*>(checkpointObject, 0x9f0);
					return gameObject ? gameObject->getPositionX() : 0.f;
				}
			}
		}
		return 0.f;
	};



	if (this->m_maxCheckpointNow <= 0) {

		if (!Mod::get()->getSettingValue<bool>("show-percentage-mod") || PlayLayer::get()->m_level->isPlatformer()) {
			this->m_plabelMedium->setString("0/0");
			return;
		}

		if (Utils::from<StartPosObject*>(playLayer, 0x2a40)) {
			this->m_plabelLeft->setString(
				cocos2d::CCString::createWithFormat("%.2f%%"
					, Utils::calculatePercentageWithFixedPosition(getFrameCheckpoint(-1), getPositionCheckpoint(-1))
				)->getCString());
			this->m_plabelMedium->setString(
				cocos2d::CCString::createWithFormat("%.2f%%",
					Utils::calculatePercentageWithFixedPosition(getFrameCheckpoint(-1), getPositionCheckpoint(-1))
				)->getCString());
			this->m_plabelMedium->limitLabelWidth(70.f, 0.5f, 0.1f);
			this->m_plabelRight->setString(
				cocos2d::CCString::createWithFormat("%.2f%%",
					Utils::calculatePercentageWithFixedPosition(getFrameCheckpoint(-1), getPositionCheckpoint(-1))
				)->getCString());
		}
		else {
			this->m_plabelLeft->setString("0.00%");
			this->m_plabelMedium->setString("0.00%");
			this->m_plabelRight->setString("0.00%");
		}

		return;
	}


	if (Mod::get()->getSettingValue<bool>("show-percentage-mod") && !PlayLayer::get()->m_level->isPlatformer()) {
		this->m_plabelLeft->setString(
			cocos2d::CCString::createWithFormat("%.2f%%",
				Utils::calculatePercentageWithFixedPosition(getFrameCheckpoint(getIndex(this->index - 1)), getPositionCheckpoint(getIndex(this->index - 1)))
			)->getCString());
		this->m_plabelMedium->setString(
			cocos2d::CCString::createWithFormat("%.2f%%",
				Utils::calculatePercentageWithFixedPosition(getFrameCheckpoint(getIndex(this->index)), getPositionCheckpoint(getIndex(this->index)))
			)->getCString());
		this->m_plabelRight->setString(
			cocos2d::CCString::createWithFormat("%.2f%%",
				Utils::calculatePercentageWithFixedPosition(getFrameCheckpoint(getIndex(this->index + 1)), getPositionCheckpoint(getIndex(this->index + 1)))
			)->getCString());
	}
	else {
		this->m_plabelMedium->setString(
			cocos2d::CCString::createWithFormat("%i/%i", this->index + 1, this->m_maxCheckpointNow)->getCString());
	}

	this->m_plabelMedium->limitLabelWidth(70.f, 0.5f, 0.1f);

}

void CheckpointSwitcher::switchCheckpoint(int i) {
	auto playLayer = PlayLayer::get();
	if (!playLayer || !playLayer->m_isPracticeMode || this->m_maxCheckpointNow <= 0 || !Mod::get()->getSettingValue<bool>("enable-mod")) return;

	if (Utils::from<CCArray*>(playLayer, 0x2e18)->count() !=
		this->m_maxCheckpointNow) {
		this->updateCheckpoints();
	}

	if (i > static_cast<int>(this->m_maxCheckpointNow - 1)) {
		this->index = -1;
	}
	else if (i < -1) {
		this->index = static_cast<int>(this->m_maxCheckpointNow - 1);
	}
	else {
		this->index = i;
	}

	//std::cout << this->index << std::endl;

	if (!Mod::get()->getSettingValue<bool>("switch-after-reset")) {
		playLayer->resetLevel();
		if (CCScheduler::get()->isTargetPaused(playLayer)) {
			auto fmodChannelGD = FMODAudioEngine::sharedEngine()->m_globalChannel;
			fmodChannelGD->setPaused(true);
		}
		playLayer->update(CCDirector::get()->getDeltaTime());
	}

	if (Mod::get()->getSettingValue<bool>("hide-interface-switch")) {
		this->onFadeInterface();
	}
	else {
		this->m_plabelLeft->stopActionByTag(0x5);
		this->m_plabelMedium->stopActionByTag(0x5);
		this->m_plabelRight->stopActionByTag(0x5);
		this->m_pleftArrowButton->stopActionByTag(0x5);
		this->m_prightArrowButton->stopActionByTag(0x5);

		this->m_plabelLeft->setOpacity(Utils::convertOpacitySimplf(1.f));
		this->m_plabelMedium->setOpacity(Utils::convertOpacitySimplf(1.f));
		this->m_plabelRight->setOpacity(Utils::convertOpacitySimplf(1.f));
		this->m_pleftArrowButton->setOpacity(Utils::convertOpacitySimplf(1.f));
		this->m_prightArrowButton->setOpacity(Utils::convertOpacitySimplf(1.f));
	}
}


void CheckpointSwitcher::updateCheckpoints() {
	auto playLayer = PlayLayer::get();
	if (!playLayer) {
		return;
	}

	if (this->m_maxCheckpointNow != Utils::from<CCArray*>(playLayer, 0x2e18)->count()) {
		this->index = std::max(static_cast<int>(Utils::from<CCArray*>(playLayer, 0x2e18)->count()) - 1, 0);
		if (Mod::get()->getSettingValue<bool>("hide-interface-switch")) {
			this->onFadeInterface();
		}
	}
	this->m_maxCheckpointNow = Utils::from<CCArray*>(playLayer, 0x2e18)->count();
	this->index = std::max(this->index, -1);
	this->updateUI();

	/*
	if (Utils::from<CCArray*>(playLayer, 0x2e18)->count() > 0) {
		auto chk = typeinfo_cast<CheckpointObject*>(Utils::from<CCArray*>(playLayer, 0x2e18)->lastObject());

		intptr_t offsets[] = {
			0x828, 0x9f0, 0x9f4, 0x9f8, 0x9fc,
			0xa00, 0xa04, 0xa08, 0xa0c, 0xa10,
			0xa14, 0xa18, 0xa1c, 0xa20, 0xa24,
			0xa28, 0xa2c, 0xa30, 0xa34, 0xa38,
			0x0a38, 0x0bdc
		};
		system("cls");

		for (intptr_t offset : offsets) {
			std::cout << "offset 0x" << std::hex << offset << ": ";

			bool bool_success = false;
			bool int_success = false;
			bool float_success = false;
			bool string_success = false;
			bool CCNode_success = false;
			bool vector_success = false;

			try {
				bool valor_bool = Utils::from<bool>(chk, offset);
				std::cout << "\tTipo: bool | Valor: " << (valor_bool ? "true" : "false") << std::endl;
				bool_success = true;
			}
			catch (const std::exception&) {
				std::cout << "\tTipo: bool | Valor: No se pudo" << std::endl;
			}

			try {
				int valor_int = Utils::from<int>(chk, offset);
				std::cout << "\t\tTipo: int | Valor: " << valor_int << std::endl;
				int_success = true;
			}
			catch (const std::exception&) {
				std::cout << "\t\tTipo: int | Valor: No se pudo" << std::endl;
			}

			try {
				float valor_float = Utils::from<float>(chk, offset);
				std::cout << "\t\tTipo: float | Valor: " << valor_float << std::endl;
				float_success = true;
			}
			catch (const std::exception&) {
				std::cout << "\t\tTipo: float | Valor: No se pudo" << std::endl;
			}

				try {
					std::cout << "\t\tTipo: std::string | Valor: " << Utils::from<void*>(chk, offset) << std::endl;
					string_success = true;
				}
				catch (const std::exception&) {
					std::cout << "\t\tTipo: std::string | Valor: No se pudo" << std::endl;
				}

				try {
					std::vector<int> valor_vector = Utils::from<std::vector<int>>(chk, offset);
					std::cout << "\t\tTipo: std::vector<int> | Valor: ";
					for (const auto& element : valor_vector) {
						std::cout << element << " ";
					}
					std::cout << std::endl;
					vector_success = true;
				}
				catch (const std::exception&) {
					std::cout << "\t\tTipo: std::vector<int> | Valor: No se pudo" << std::endl;
				}


			try {
				CCNode* valor_CCNode = Utils::from<CCNode*>(chk, offset);
				auto name = Utils::getNameObj(valor_CCNode);
				std::cout << "\t\tTipo: CCNode* | Nombre: " << name << std::endl;
				CCNode_success = true;
			}
			catch (const std::exception&) {
				std::cout << "\t\tTipo: CCNode* | Nombre: No se pudo" << std::endl;
			}

			if (!bool_success && !int_success && !float_success && string_success && !CCNode_success) {
				std::cout << "\t\tTipo de dato no compatible" << std::endl;
			}
		}
	}
	*/
}

void CheckpointSwitcher::onLeftCheckpoint(cocos2d::CCObject* pSender) {
	if (PlayLayer::get() && Utils::from<bool>(PlayLayer::get(), 0x2c28))return;
	if (!Mod::get()->getSettingValue<bool>("enable-mod")) {
		return;
	}
	this->switchCheckpoint(this->index - 1);
	this->updateUI();
}

void CheckpointSwitcher::onRightCheckpoint(cocos2d::CCObject* pSender) {
	if (PlayLayer::get() && Utils::from<bool>(PlayLayer::get(), 0x2c28))return;
	if (!Mod::get()->getSettingValue<bool>("enable-mod")) {
		return;
	}
	this->switchCheckpoint(this->index + 1);
	this->updateUI();
}

void CheckpointSwitcher::onFadeInterface() {
	auto playLayer = PlayLayer::get();
	if (!playLayer)return;

	this->m_plabelLeft->stopActionByTag(0x5);
	this->m_plabelMedium->stopActionByTag(0x5);
	this->m_plabelRight->stopActionByTag(0x5);
	this->m_pleftArrowButton->stopActionByTag(0x5);
	this->m_prightArrowButton->stopActionByTag(0x5);

	auto fade = cocos2d::CCSequence::create(cocos2d::CCFadeTo::create(0.f, Utils::convertOpacitySimplf(1.f)),
		cocos2d::CCFadeTo::create(1.f, Utils::convertOpacitySimplf(Mod::get()->getSettingValue<bool>("hide-interface-switch") ? 0.f :
			1.f)), nullptr);

	fade->setTag(0x5);
	this->m_plabelLeft->runAction(reinterpret_cast<cocos2d::CCSequence*>(fade->copy()));
	this->m_plabelMedium->runAction(reinterpret_cast<cocos2d::CCSequence*>(fade->copy()));
	this->m_plabelRight->runAction(reinterpret_cast<cocos2d::CCSequence*>(fade->copy()));
	this->m_pleftArrowButton->runAction(reinterpret_cast<cocos2d::CCSequence*>(fade->copy()));
	this->m_prightArrowButton->runAction(reinterpret_cast<cocos2d::CCSequence*>(fade->copy()));
}

class $modify(PlayLayer) {

	int lastCountCheckpoints = 0;

	CheckpointSwitcher* getCheckpointSwitcher() {
		return typeinfo_cast<CheckpointSwitcher*>(this->getChildByID("CheckpointSwitcher_"));
	}

	void onLeftCheckpointSwitcherUI() {
		if (!PlayLayer::get() || Utils::from<bool>(PlayLayer::get(), 0x2c28))return;
		auto sps = getCheckpointSwitcher();
		if (sps) {
			sps->onLeftCheckpoint(nullptr);
		}
	}

	void onRightCheckpointUI() {
		if (!PlayLayer::get() || Utils::from<bool>(PlayLayer::get(), 0x2c28))return;
		auto sps = getCheckpointSwitcher();
		if (sps) {
			sps->onRightCheckpoint(nullptr);
		}
	}


	void updateCheckpointInCheckpointSwitcher() {
		auto chvariable = getCheckpointSwitcher();
		if (chvariable) {
			chvariable->updateCheckpoints();
			chvariable->updateUI();
		}
	}

	void togglePracticeMode(bool p0) {
		PlayLayer::togglePracticeMode(p0);
		updateCheckpointInCheckpointSwitcher();
	}

	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {

		if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
			return false;
		}

		this->m_uiLayer->template addEventListener<keybinds::InvokeBindFilter>([=](keybinds::InvokeBindEvent* event) {
			if (event->isDown()) {
				onLeftCheckpointSwitcherUI();
			}
			return ListenerResult::Propagate;
			}, "switch-left-checkpoint-switcher"_spr);

		this->m_uiLayer->template addEventListener<keybinds::InvokeBindFilter>([=](keybinds::InvokeBindEvent* event) {
			if (event->isDown()) {
				onRightCheckpointUI();
			}
			return ListenerResult::Propagate;
			}, "switch-right-checkpoint-switcher"_spr);

		auto CheckpointSwitcherM = CheckpointSwitcher::create();
		if (CheckpointSwitcherM) {
			CheckpointSwitcherM->updateUI();
			CheckpointSwitcherM->setID("CheckpointSwitcher_");
			this->addChild(CheckpointSwitcherM, -1);
		}


		return true;
	}

	void postUpdate(float p0) {
		PlayLayer::postUpdate(p0);


		if (m_fields->lastCountCheckpoints != Utils::from<CCArray*>(this, 0x2e18)->count()) {
			updateCheckpointInCheckpointSwitcher();
		}

		m_fields->lastCountCheckpoints = Utils::from<CCArray*>(this, 0x2e18)->count();
	}

	TodoReturn removeCheckpoint(bool p0) {
		PlayLayer::removeCheckpoint(p0);
		updateCheckpointInCheckpointSwitcher();
	}

	void resetLevel() {
		PlayLayer::resetLevel();
		updateCheckpointInCheckpointSwitcher();
	}

	TodoReturn loadFromCheckpoint(CheckpointObject* p0) {
		auto checkSwitch = reinterpret_cast<CheckpointSwitcher*>(getCheckpointSwitcher());
		bool hasCheckSwitch = checkSwitch && Mod::get()->getSettingValue<bool>("enable-mod");
		bool hasCheckpoints = Utils::from<CCArray*>(this, 0x2e18)->count() > 0;

		if (checkSwitch && hasCheckSwitch && hasCheckpoints && checkSwitch->index > -1) {
			p0 = reinterpret_cast<CheckpointObject*>(Utils::from<CCArray*>(this, 0x2e18)->objectAtIndex(checkSwitch->index));
		}
		else if (checkSwitch && hasCheckSwitch && !hasCheckpoints || checkSwitch && checkSwitch->index <= -1) {
			Utils::from<float>(this, 0x2c20) = 0.f;
			Utils::from<void*>(this, 0x2e14) = 0;
			StartPosObject* startPos = Utils::from<StartPosObject*>(this, 0x2a40);
			if (startPos) {
				startPos->retain();
				this->setStartPosObject(nullptr);
				this->setupLevelStart(Utils::from<LevelSettingsObject*>(startPos, 0x678));
				this->setStartPosObject(startPos);
				startPos->release();
				return;
			}
			this->setupLevelStart(this->m_levelSettings);
			this->startMusic();
			return;
		}

		PlayLayer::loadFromCheckpoint(p0);
	}
};

class $modify(UILayer) {

	CheckpointSwitcher* getCheckpointSwitcher() {
		return typeinfo_cast<CheckpointSwitcher*>(PlayLayer::get()->getChildByID("CheckpointSwitcher_"));
	}


	void updateCheckpointInCheckpointSwitcher() {
		auto chvariable = getCheckpointSwitcher();
		if (chvariable) {
			chvariable->updateCheckpoints();
			chvariable->updateUI();
		}
	}

	void onCheck(cocos2d::CCObject * sender) {
		UILayer::onCheck(sender);
		updateCheckpointInCheckpointSwitcher();
	}

	void onDeleteCheck(cocos2d::CCObject* sender) {
		UILayer::onDeleteCheck(sender);
		updateCheckpointInCheckpointSwitcher();
	}

	
};

class $modify(PlayerObject) {

	CheckpointSwitcher* getCheckpointSwitcher() {
		return typeinfo_cast<CheckpointSwitcher*>(PlayLayer::get()->getChildByID("CheckpointSwitcher_"));
	}

	void updateCheckpointInCheckpointSwitcher() {
		auto chvariable = getCheckpointSwitcher();
		if (chvariable) {
			chvariable->updateCheckpoints();
			chvariable->updateUI();
		}
	}

	TodoReturn tryPlaceCheckpoint() {
		PlayerObject::tryPlaceCheckpoint();
		updateCheckpointInCheckpointSwitcher();
	}
};

$on_mod(Loaded) {
#ifdef GEODE_IS_WINDOWS
	//matdash::create_console();
#endif
}