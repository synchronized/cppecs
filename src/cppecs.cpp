#include <iostream>

#include "cppecs/cppecs.hpp"

namespace cppecs {

void World::StartUp() {
    std::shared_ptr<Commands> commands = createCommands();
    Queryer queryer(*this);
    for (auto& sys : m_startUpSystems) {
        sys(*commands, queryer);
    }
}

void World::Update() {
    std::shared_ptr<Commands> commands = createCommands();
    Queryer queryer(*this);
    for (auto& sys : m_systems) {
        sys(*commands, queryer);
    }

    for (auto commands : m_commands) {
        commands->Execute();
    }
    destoryCommands();
}

std::shared_ptr<Commands> World::createCommands() {
    if (!m_commandsCache.empty()) {
        m_commands.push_back(m_commandsCache.back());
        m_commandsCache.pop_back();
    } else {
        std::shared_ptr<Commands> commands;
        commands.reset(new Commands(*this));
        m_commands.push_back(commands);
    }
    return m_commands.back();
}


}