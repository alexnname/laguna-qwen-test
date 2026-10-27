// plugin.h
#pragma once

#include <string>
#include <vector>

#ifdef _WIN32
    #define PLUGIN_API __declspec(dllexport)
#else
    #define PLUGIN_API __attribute__((visibility("default")))
#endif

namespace plugin {

struct PluginInfo {
    std::string name;
    std::string version;
    std::string author;
};

class IPlugin {
public:
    virtual ~IPlugin() = default;
    
    // Основные методы жизненного цикла
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    
    // Информация о плагине
    virtual PluginInfo getInfo() const = 0;
    
    // Основные функции (переопределяются в зависимости от задач)
    virtual void execute() = 0;
};

} // namespace plugin

// Внешние функции для работы с плагином
extern "C" {
    PLUGIN_API plugin::IPlugin* createPlugin();
    PLUGIN_API void destroyPlugin(plugin::IPlugin* plugin);
    PLUGIN_API const char* getPluginInfo();
}
