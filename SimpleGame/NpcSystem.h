#pragma once

#include "World.h"
#include <filesystem>
#include <string>
#include <vector>

struct Npc
{
    int id;
    std::wstring name;
    std::wstring role;
    double x, y;
    int style;
    unsigned int meetings = 0;
};

// Authored prototype residents, not death-generated NPCs or an AI service.
class NpcSystem
{
public:
    explicit NpcSystem(const std::filesystem::path& saveDirectory);
    const std::vector<Npc>& Residents() const { return residents_; }
    int Nearest(double x, double y, const World& world) const;
    bool CanWalk(double x, double y, const World& world) const;
    bool IsPresent(const Npc& npc, const World& world) const;
    std::vector<std::wstring> Conversation(int index, const World& world);
    const std::string& Error() const { return error_; }
private:
    bool Save();
    void Load();
    std::vector<Npc> residents_;
    std::filesystem::path savePath_;
    std::string error_;
};
