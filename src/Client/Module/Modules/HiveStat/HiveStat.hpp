#pragma once

#include "../Module.hpp"
#include "../Misc/HiveModeCatcher/HiveModeCatcherListener.hpp"
#include "../../../../Utils/Hive.hpp"


class HiveStat : public Module {
private:
    std::vector<std::pair<std::string, Hive::PlayerStats>> playerStatsList;
    std::vector<std::string> queueList;
    std::shared_mutex queueMutex;
    std::condition_variable_any queueCondition;
    std::thread fetchThread;
    bool stopThread = false;
    bool renderOverlay = true;
    Vec2<float> currentPos;
    bool shoudReset = false;
public:
    HiveStat() : Module("HiveOverlay", "",
                      IDR_RE_Q_PNG, "O") {
        Module::setup();
    };

    void onEnable() override {
        Listen(this, RenderEvent, &HiveStat::onRender)
        Listen(this, PacketEvent, &HiveStat::onPacketReceive)
        Listen(this, KeyEvent, &HiveStat::onKey)
        fetchThread = std::thread(&HiveStat::workerThread, this);
        Module::onEnable();
        FlarialGUI::Notify("Hive Overlay works only in sky, bed, mm, ctf");
        FlarialGUI::Notify("To change the position of Hive Overlay, Please click " +
                           ModuleManager::getModule("ClickGUI")->settings.getSettingByName<std::string>(
                                   "editmenubind")->value + " in the settings tab.");
    }

    void onDisable() override {
        stopThread = true;
        Deafen(this, RenderEvent, &HiveStat::onRender)
        Deafen(this, PacketEvent, &HiveStat::onPacketReceive)
        Deafen(this, KeyEvent, &HiveStat::onKey)
        Module::onDisable();
    }

    void defaultConfig() override {
        if (settings.getSettingByName<std::string>("Overlay") == nullptr)
            settings.addSetting("Overlay", (std::string) "O");

    }

    void settingsRender(float settingsOffset) override {
        float x = Constraints::PercentageConstraint(0.019, "left");
        float y = Constraints::PercentageConstraint(0.10, "top");


        this->addHeader("Keybinds");
        this->addKeybind("Toggle Overlay Keybind", "When setting, hold the new bind for 2 seconds", settings.getSettingByName<std::string>("Overlay")->value);
        this->resetPadding();
    }

    void fetchPlayerStats(const std::string& playerName) {

        std::string cg = HiveModeCatcherListener::currentGame;

        size_t dashPos = cg.find('-');

        if (dashPos != std::string::npos) {
            cg = cg.substr(0, dashPos);
        }

        std::transform(cg.begin(), cg.end(), cg.begin(), ::tolower);
        Hive::PlayerStats stats = Hive::GetStats(cg, playerName);
        std::unique_lock<std::shared_mutex> lock(queueMutex);
        playerStatsList.emplace_back(playerName, stats);
    }

    void workerThread() {
        while (!stopThread) {
            std::string playerName;

            {
                std::unique_lock<std::shared_mutex> lock(queueMutex);
                queueCondition.wait(lock, [this] { return !queueList.empty() || stopThread; });

                if (stopThread && queueList.empty()) break;

                playerName = queueList.front();
                queueList.erase(queueList.begin());
            }

            fetchPlayerStats(playerName);
        }
    }

    void onRender(RenderEvent &event) {


        if (ClientInstance::getTopScreenName() == "hud_screen" &&
            this->isEnabled()) {



            Vec2<float> settingperc = Vec2<float>(this->settings.getSettingByName<float>("percentageX")->value,
                                                  this->settings.getSettingByName<float>("percentageY")->value);



            if (settingperc.x != 0)
        currentPos = Vec2<float>(settingperc.x * (MC::windowSize.x - 150), settingperc.y * (MC::windowSize.y - 50));
            else
                currentPos = Constraints::CenterConstraint(150, 150);

            if (ClickGUI::editmenu) {
                FlarialGUI::SetWindowRect(currentPos.x, currentPos.y, 150, 50, 123);
                FlarialGUI::FlarialTextWithFont(currentPos.x, currentPos.y,
                                                L"Overlay", 150, 50,
                                                DWRITE_TEXT_ALIGNMENT_LEADING, Constraints::SpacingConstraint(1.05, 150),
                                                DWRITE_FONT_WEIGHT_NORMAL);
            }

            Vec2<float> vec2;
            vec2 = FlarialGUI::CalculateMovedXY(currentPos.x, currentPos.y, 123, 150, 50);

            currentPos.x = vec2.x;
            currentPos.y = vec2.y;

            Vec2<float> percentages = Constraints::CalculatePercentage(currentPos.x, currentPos.y, 150, 50);

            this->settings.setValue("percentageX", percentages.x);
            this->settings.setValue("percentageY", percentages.y);

            if (ClickGUI::editmenu) {
                FlarialGUI::UnsetWindowRect();
            }
        }

        if (SDK::getCurrentScreen() != "hud_screen") ClickGUI::editmenu = false;


        if(!renderOverlay) return;


        std::string cg = HiveModeCatcherListener::currentGame;

        size_t dashPos = cg.find('-');

        if (dashPos != std::string::npos) {
            cg = cg.substr(0, dashPos);
        }

        std::transform(cg.begin(), cg.end(), cg.begin(), ::tolower);
        if (cg != "bed" && cg != "sky" && cg != "murder" && cg != "ctf") {
            return;
        }


        if (!SDK::clientInstance || !SDK::clientInstance->getLocalPlayer() ||
            SDK::getCurrentScreen() != "hud_screen" ||
            !SDK::clientInstance->getLocalPlayer()->getLevel()) {
            return;
        }


        ImVec2 position(currentPos.x, currentPos.y);
        ImGui::SetNextWindowPos(position, ImGuiCond_Always);
        ImGui::Begin("Overlay", nullptr,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

        if (ImGui::BeginTable("Player Stats", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersV)) {
            ImGui::TableSetupColumn("Player Name");
            ImGui::TableSetupColumn("FKDR");
            ImGui::TableSetupColumn("KD");
            ImGui::TableSetupColumn("Win Rate");
            ImGui::TableSetupColumn("Level");
            ImGui::TableHeadersRow();

            for (const auto &pair: SDK::clientInstance->getLocalPlayer()->getLevel()->getPlayerMap()) {
                std::string name = String::removeNonAlphanumeric(String::removeColorCodes(pair.second.name));

                auto it = std::find_if(playerStatsList.begin(), playerStatsList.end(),
                                       [&name](const std::pair<std::string, Hive::PlayerStats> &element) {
                                           return element.first == name;
                                       });

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", name.c_str());

                ImGui::TableSetColumnIndex(1);
                if (it != playerStatsList.end()) {
                    if( it->second.getCode() == 0) {
                        {
                            std::ostringstream oss;
                            oss << std::fixed << std::setprecision(2) << it->second.getFKDR();
                            ImGui::Text("%s", oss.str().c_str());
                        }
                        {
                            ImGui::TableSetColumnIndex(2);
                            std::ostringstream oss;
                            oss << std::fixed << std::setprecision(2) << it->second.getKD();
                            ImGui::Text("%s", oss.str().c_str());
                        }
                        {
                            ImGui::TableSetColumnIndex(3);
                            std::ostringstream oss;
                            oss << std::fixed << std::setprecision(0) << it->second.getWinRate();
                            ImGui::Text("%s", oss.str().c_str());
                        }
                        {
                            ImGui::TableSetColumnIndex(4);
                            std::ostringstream oss;
                            oss << std::fixed << std::setprecision(0) << it->second.getLevel();
                            ImGui::Text("%s", oss.str().c_str());
                        }
                    }else{
                        switch (it->second.getCode()){
                            case 1:
                                ImGui::Text("err");
                                ImGui::TableSetColumnIndex(2);
                                ImGui::Text("nick?");
                                ImGui::TableSetColumnIndex(3);
                                ImGui::Text("-");
                                ImGui::TableSetColumnIndex(4);
                                ImGui::Text("-");
                            case 2:
                                ImGui::Text("err");
                                ImGui::TableSetColumnIndex(2);
                                ImGui::Text("missing data");
                                ImGui::TableSetColumnIndex(3);
                                ImGui::Text("-");
                                ImGui::TableSetColumnIndex(4);
                                ImGui::Text("-");
                        }
                    }
                } else {
                    std::unique_lock<std::shared_mutex> lock(queueMutex);
                    if (std::find(queueList.begin(), queueList.end(), name) == queueList.end()) {
                        queueList.push_back(name);
                        queueCondition.notify_one();
                    }
                    ImGui::Text("Loading stats...");
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("-");
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("-");
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("-");
                }
            }

            ImGui::EndTable();
        }

        ImGui::End();
    }

    void onPacketReceive(PacketEvent &event) {
        if (SDK::getServerIP().find("hive") == std::string::npos) return;
        MinecraftPacketIds id = event.getPacket()->getId();

        if (shoudReset) {
            if (id == MinecraftPacketIds::ChangeDimension) {
                std::unique_lock<std::shared_mutex> lock(queueMutex);
                playerStatsList.clear();
                queueList.clear();
            }
        } else {
            shoudReset = true;
        }
    }

    void onKey(KeyEvent &event) {

        if (event.getKey() == Utils::getStringAsKey(this->settings.getSettingByName<std::string>("Overlay")->value) &&
            static_cast<ActionType>(event.getAction()) == ActionType::Released) {
            renderOverlay = !renderOverlay;
            if(renderOverlay){
                playerStatsList.clear();
                queueList.clear();
            }
        }

    }
};

