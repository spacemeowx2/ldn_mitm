#define TESLA_INIT_IMPL // If you have more than one file using the tesla header, only define this in the main one
#include <tesla.hpp>    // The Tesla Header
#include "ldn.h"

enum class State {
    Uninit,
    Error,
    Loaded,
};

Service g_ldnSrv;
LdnMitmConfigService g_ldnConfig;
State g_state;
char g_version[32];

class DisabledToggleListItem : public tsl::elm::ToggleListItem {
public:
    DisabledToggleListItem() : ToggleListItem("Disabled", false) {
        u32 enabled;
        Result rc;

        rc = ldnMitmGetEnabled(&g_ldnConfig, &enabled);
        if (R_FAILED(rc)) {
            g_state = State::Error;
        }

        this->setState(!enabled);

        this->setStateChangedListener([](bool enabled) {
            Result rc = ldnMitmSetEnabled(&g_ldnConfig, !enabled);
            if (R_FAILED(rc)) {
                g_state = State::Error;
            }
        });
    }
};

class LoggingToggleListItem : public tsl::elm::ToggleListItem {
public:
    LoggingToggleListItem() : ToggleListItem("Logging", false) {
        u32 enabled;
        Result rc;

        rc = ldnMitmGetLogging(&g_ldnConfig, &enabled);
        if (R_FAILED(rc)) {
            g_state = State::Error;
        }

        this->setState(enabled);

        this->setStateChangedListener([](bool enabled) {
            Result rc = ldnMitmSetLogging(&g_ldnConfig, enabled);
            if (R_FAILED(rc)) {
                g_state = State::Error;
            }
        });
    }
};

class MainGui : public tsl::Gui {
public:
    MainGui() { }

    virtual tsl::elm::Element* createUI() override {
        auto frame = new tsl::elm::OverlayFrame("ldn_mitm", g_version);

        auto list = new tsl::elm::List();

        if (g_state == State::Error) {
            list->addItem(new tsl::elm::ListItem("ldn_mitm is not loaded."));
        } else if (g_state == State::Uninit) {
            list->addItem(new tsl::elm::ListItem("wrong state"));
        } else {
            list->addItem(new DisabledToggleListItem());
            list->addItem(new LoggingToggleListItem());
        }

        frame->setContent(list);
        
        return frame;
    }

    // Called once every frame to update values
    virtual void update() override {

    }

    // Called once every frame to handle inputs not handled by other UI elements
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) {
        return false;
    }
};

class Overlay : public tsl::Overlay {
public:
    virtual void initServices() override {
        g_state = State::Uninit;
        tsl::hlp::doWithSmSession([&] {
            Result rc;

            rc = smGetService(&g_ldnSrv, "ldn:u");
            if (R_FAILED(rc)) {
                g_state = State::Error;
                return;
            }

            rc = ldnMitmGetConfigFromService(&g_ldnSrv, &g_ldnConfig);
            if (R_FAILED(rc)) {
                g_state = State::Error;
                return;
            }

            rc = ldnMitmGetVersion(&g_ldnConfig, g_version);
            if (R_FAILED(rc)) {
                strcpy(g_version, "Error");
            }

            g_state = State::Loaded;
        });

    }
    virtual void exitServices() override {
        serviceClose(&g_ldnConfig.s);
        serviceClose(&g_ldnSrv);
    }

    virtual void onShow() override {}    // Called before overlay wants to change from invisible to visible state
    virtual void onHide() override {}    // Called before overlay wants to change from visible to invisible state

    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<MainGui>();  // Initial Gui to load. It's possible to pass arguments to it's constructor like this
    }
};

int main(int argc, char **argv) {
    return tsl::loop<Overlay>(argc, argv);
}
