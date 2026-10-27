// plugin.cpp
#include "plugin.h"
#include <iostream>

namespace plugin {

class BasicPlugin : public IPlugin {
public:
    BasicPlugin() : initialized_(false) {}
    
    bool initialize() override {
        std::cout << "Initializing plugin: " << info_.name << std::endl;
        initialized_ = true;
        return true;
    }
    
    void shutdown() override {
        std::cout << "Shutting down plugin: " << info_.name << std::endl;
        initialized_ = false;
    }
    
    PluginInfo getInfo() const override {
        return info_;
    }
    
    void execute() override {
        if (initialized_) {
            std::cout << "Executing plugin: " << info_.name << std::endl;
            // Ваш код здесь
        }
    }
    
private:
    bool initialized_;
    PluginInfo info_{
        "BasicPlugin",
        "1.0.0",
        "Your Name"
    };
};

} // namespace plugin

// Внешние функции
extern "C" {
    
    PLUGIN_API plugin::IPlugin* createPlugin() {
        return new plugin::BasicPlugin();
    }
    
    PLUGIN_API void destroyPlugin(plugin::IPlugin* plugin) {
        delete plugin;
    }
    
    PLUGIN_API const char* getPluginInfo() {
        static std::string info = 
            "Name: BasicPlugin\n"
            "Version: 1.0.0\n"
            "Author: Your Name";
        return info.c_str();
    }
}
