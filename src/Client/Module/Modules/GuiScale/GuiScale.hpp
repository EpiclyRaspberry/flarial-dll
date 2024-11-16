#pragma once

#include "../Module.hpp"

class GuiScale : public Module {
private:
    float originalScale = 0.f;
    bool restored = false;
public:
    static inline bool fixResize = false;
    GuiScale() : Module("MC GUI Scale", "Change your GUI Scale beyond\nMinecraft's restrictions.",
                        IDR_SCALE_PNG, "") {
        Module::setup();
    };

    void onEnable() override {
        restored = false;
        Listen(this, SetupAndRenderEvent, &GuiScale::onSetupAndRender)
        Module::onEnable();
    }

    void onDisable() override {
        if(!restored) {
            delayDisable = true;
            return;
        }
        Deafen(this, SetupAndRenderEvent, &GuiScale::onSetupAndRender)

        Module::onDisable();
    }

    void defaultConfig() override {
        if (settings.getSettingByName<float>("guiscale") == nullptr) settings.addSetting("guiscale", 2.0f);
    }

    void settingsRender(float settingsOffset) override {

        float toggleX = Constraints::PercentageConstraint(0.019, "left");
        float toggleY = Constraints::PercentageConstraint(0.10, "top");

        const float textWidth = Constraints::RelativeConstraint(0.12, "height", true);
        const float textHeight = Constraints::RelativeConstraint(0.029, "height", true);

        FlarialGUI::FlarialTextWithFont(toggleX, toggleY, L"UI Scale", textWidth * 3.0f, textHeight,
                                        DWRITE_TEXT_ALIGNMENT_LEADING,
                                        Constraints::RelativeConstraint(0.12, "height", true),
                                        DWRITE_FONT_WEIGHT_NORMAL);


        float percent = FlarialGUI::Slider(4, toggleX + FlarialGUI::SettingsTextWidth("UI Scale "),
                                           toggleY, this->settings.getSettingByName<float>("guiscale")->value, 4.0f, 1.0f);

        this->settings.getSettingByName<float>("guiscale")->value = percent;

    }

    void onSetupAndRender(SetupAndRenderEvent &event) {
        update();
    };

    void update() {
        float targetScale = delayDisable ? originalScale : this->settings.getSettingByName<float>("guiscale")->value;
        auto guiData = SDK::clientInstance->getGuiData();
        if(targetScale == guiData->GuiScale && !delayDisable && !fixResize) return;

        updateScale(targetScale);
    }

    void updateScale(float newScale) {
        if(restored && !fixResize) return;

        fixResize = false;
        auto guiData = SDK::clientInstance->getGuiData();

        if(originalScale == 0) {
            originalScale = guiData->GuiScale;
        }
        float oldScale = guiData->GuiScale;

        auto screenSize = guiData->ScreenSize;
        static auto safeZone = Vec2<float>{ 0.f, 0.f };

        SDK::clientInstance->_updateScreenSizeVariables(&screenSize, &safeZone, newScale < 1.f ? 1.f : newScale);

        if(delayDisable) {
            delayDisable = false;
            restored = true;
        }
    }
};
