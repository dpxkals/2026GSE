#include "stdafx.h"
#include "WorldMonitor.h"
#include "MonitorServer.h"
#include "WorldSimulation.h"
#include "LevelOne.h"
#include "NpcSystem.h"
#include "VillageLayout.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>

namespace
{
    long long UnixMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    std::string Quote(const std::string& text)
    {
        std::ostringstream out;
        out << '"';
        for (unsigned char c : text)
        {
            if (c == '"' || c == '\\')
            {
                out << '\\' << char(c);
            }
            else if (c < 32)
            {
                out << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << int(c);
            }
            else
            {
                out << char(c);
            }
        }
        out << '"';
        return out.str();
    }

    std::string Quote(const std::wstring& text)
    {
        if (text.empty())
        {
            return "\"\"";
        }
        int count = WideCharToMultiByte(CP_UTF8,
                                        WC_ERR_INVALID_CHARS,
                                        text.data(),
                                        int(text.size()),
                                        nullptr,
                                        0,
                                        nullptr,
                                        nullptr);
        if (count <= 0)
        {
            return Quote(std::string("Invalid Unicode"));
        }
        std::string utf8(count, '\0');
        WideCharToMultiByte(CP_UTF8,
                            WC_ERR_INVALID_CHARS,
                            text.data(),
                            int(text.size()),
                            utf8.data(),
                            count,
                            nullptr,
                            nullptr);
        return Quote(utf8);
    }

    std::string Number(double value)
    {
        if (!std::isfinite(value))
        {
            return "null";
        }
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(10) << value;
        return out.str();
    }

    std::string Position(WorldPoint p)
    {
        return "{\"x\":" + Number(p.x) + ",\"y\":" + Number(p.y) + "}";
    }

    template <typename Container> std::string Array(const Container& values)
    {
        std::string result = "[";
        bool first = true;
        for (double value : values)
        {
            if (!first)
            {
                result += ',';
            }
            first = false;
            result += Number(value);
        }
        return result + "]";
    }

    bool Loaded(const World& world, WorldPoint p)
    {
        return world.Find(WorldInt(std::floor(p.x)), WorldInt(std::floor(p.y))) != nullptr;
    }

    // Assign each tracked monster to at most one village, within its hunting grounds.
    int MonsterRegion(WorldPoint p)
    {
        int result = -1;
        double nearest = 30;
        for (int i = 0; i < int(Villages().size()); ++i)
        {
            auto center = Villages()[i].center;
            double distance = std::hypot(p.x - center.x, p.y - center.y);
            if (distance < nearest)
            {
                nearest = distance;
                result = i;
            }
        }
        return result;
    }

    struct VillageStats
    {
        int alive = 0, dead = 0, present = 0, hostile = 0, wounded = 0;
        int residents = 0, souls = 0, monsters = 0;
        long long gold = 0, goods = 0, insight = 0;
        double healthRatio = 0;
        std::array<int, static_cast<size_t>(NpcAction::Count)> actions{};
    };
}

WorldMonitor::WorldMonitor(const std::filesystem::path& directory,
                           const std::filesystem::path& assets)
    : path_(directory / L"monitor.json"),
      session_(std::to_string(GetCurrentProcessId()) + "-" + std::to_string(UnixMs()))
{
    try
    {
        server_ = std::make_unique<MonitorServer>(assets, Quote(path_.wstring()));
    }
    catch (const std::exception& error)
    {
        std::cerr << "[Monitor HTTP] Disabled: " << error.what() << '\n';
    }
    writer_ = std::thread(&WorldMonitor::WriteLoop, this);
    std::wcout << L"[Monitor] JSON snapshot: " << path_.c_str() << L'\n';
}

WorldMonitor::~WorldMonitor()
{
    {
        std::lock_guard<std::mutex> guard(mutex_);
        stopping_ = true;
    }
    ready_.notify_one();
    if (writer_.joinable())
    {
        writer_.join();
    }
}

void WorldMonitor::Publish(const World& world,
                           const WorldSimulation& society,
                           const LevelOne& level,
                           const NpcSystem& residents,
                           bool running,
                           bool paused,
                           bool focused,
                           bool force)
{
    auto now = std::chrono::steady_clock::now();
    if (!force && now < next_)
    {
        return;
    }
    next_ = now + std::chrono::seconds(1);
    try
    {
        std::array<VillageStats, VillageCount> villages{};
        int alive = 0, dead = 0, monsters = 0, loadedMonsters = 0, outsideMonsters = 0;
        std::array<int, 3> monsterKinds{}, lod{};
        std::ostringstream npcJson, mobJson;
        npcJson.imbue(std::locale::classic());
        mobJson.imbue(std::locale::classic());
        bool first = true;
        auto separator = [&]()
        {
            if (!first)
            {
                npcJson << ',';
            }
            first = false;
        };
        for (const auto& n : society.People())
        {
            auto& home = villages.at(n.home);
            if (n.health > 0)
            {
                ++alive;
                ++home.alive;
                ++lod.at(n.lod);
                home.gold += n.gold;
                home.goods += n.goods;
                home.insight += n.insight;
                home.healthRatio += n.health / n.maxHealth;
                home.hostile += n.hostility > 0 ? 1 : 0;
                home.wounded += n.health < n.maxHealth * 0.5 ? 1 : 0;
                ++home.actions.at(static_cast<size_t>(n.action));
                int inside = VillageAt(n.position);
                if (inside >= 0)
                {
                    ++villages[inside].present;
                }
            }
            else
            {
                ++dead;
                ++home.dead;
            }
            separator();
            npcJson << "{\"id\":" << Quote("citizen:" + std::to_string(n.id))
                    << ",\"kind\":\"citizen\",\"name\":" << Quote(WorldSimulation::Name(n))
                    << ",\"alive\":" << (n.health > 0 ? "true" : "false")
                    << ",\"homeVillage\":" << n.home
                    << ",\"insideVillage\":" << VillageAt(n.position)
                    << ",\"position\":" << Position(n.position)
                    << ",\"destination\":" << Position(n.destination)
                    << ",\"loaded\":" << (Loaded(world, n.position) ? "true" : "false")
                    << ",\"hp\":" << Number(n.health) << ",\"maxHp\":" << Number(n.maxHealth)
                    << ",\"level\":" << n.level << ",\"xp\":" << n.experience
                    << ",\"gold\":" << n.gold << ",\"goods\":" << n.goods
                    << ",\"insight\":" << n.insight
                    << ",\"action\":" << Quote(std::wstring(WorldSimulation::ActionName(n.action)))
                    << ",\"actionId\":" << int(n.action)
                    << ",\"goal\":" << Quote(std::wstring(WorldSimulation::GoalName(n.goal)))
                    << ",\"tendency\":"
                    << Quote(std::wstring(
                           WorldSimulation::TendencyName(WorldSimulation::Tendency(n))))
                    << ",\"hostility\":" << Number(n.hostility)
                    << ",\"replacementIn\":" << Number(n.deadTime) << ",\"lod\":" << n.lod
                    << ",\"targetKind\":" << int(n.targetKind)
                    << ",\"targetId\":" << Quote(std::to_string(n.target))
                    << ",\"personality\":" << Array(n.personality)
                    << ",\"weights\":" << Array(n.weights) << ",\"learned\":" << Array(n.learned)
                    << '}';
        }
        for (const auto& n : residents.Residents())
        {
            WorldPoint p{n.x, n.y};
            int inside = VillageAt(p);
            if (inside >= 0)
            {
                ++villages[inside].residents;
            }
            separator();
            npcJson << "{\"id\":" << Quote("resident:" + std::to_string(n.id))
                    << ",\"kind\":\"resident\",\"name\":" << Quote(n.name)
                    << ",\"alive\":true,\"homeVillage\":null,\"insideVillage\":" << inside
                    << ",\"position\":" << Position(p) << ",\"hp\":null,\"maxHp\":null"
                    << ",\"loaded\":" << (residents.IsPresent(n, world) ? "true" : "false")
                    << ",\"action\":\"안내 / 대화\",\"goal\":" << Quote(n.role)
                    << ",\"meetings\":" << n.meetings << '}';
        }
        for (const auto& n : level.Souls())
        {
            int inside = VillageAt(n.position);
            if (inside >= 0)
            {
                ++villages[inside].souls;
            }
            separator();
            npcJson << "{\"id\":" << Quote("soul:" + std::to_string(n.id))
                    << ",\"kind\":\"soul\",\"name\":" << Quote(n.Name())
                    << ",\"alive\":true,\"homeVillage\":null,\"insideVillage\":" << inside
                    << ",\"position\":" << Position(n.position) << ",\"hp\":null,\"maxHp\":null"
                    << ",\"loaded\":" << (Loaded(world, n.position) ? "true" : "false")
                    << ",\"action\":\"영혼 / 대화\",\"goal\":" << Quote(n.Role())
                    << ",\"level\":" << n.inheritedLevel << ",\"meetings\":" << n.meetings
                    << ",\"deathNumber\":" << n.deathNumber << '}';
        }
        for (const auto& n : level.Enemies())
        {
            if (n.health <= 0)
            {
                continue;
            }
            if (monsters++)
            {
                mobJson << ',';
            }
            ++monsterKinds.at(static_cast<size_t>(n.kind));
            bool loaded = Loaded(world, n.position);
            loadedMonsters += loaded ? 1 : 0;
            int region = MonsterRegion(n.position);
            if (region >= 0)
            {
                ++villages[region].monsters;
            }
            else
            {
                ++outsideMonsters;
            }
            mobJson << "{\"id\":" << Quote(std::to_string(n.id)) << ",\"kind\":" << int(n.kind)
                    << ",\"position\":" << Position(n.position) << ",\"hp\":" << Number(n.health)
                    << ",\"maxHp\":" << Number(n.maxHealth) << ",\"village\":" << region
                    << ",\"loaded\":" << (loaded ? "true" : "false") << '}';
        }
        std::ostringstream out;
        out.imbue(std::locale::classic());
        bool error = !world.Error().empty() || !society.Error().empty() || !level.Error().empty() ||
                     !residents.Error().empty();
        out << "{\"schemaVersion\":1,\"type\":\"world.snapshot\",\"session\":" << Quote(session_)
            << ",\"sequence\":" << ++sequence_ << ",\"publishedAtMs\":" << UnixMs()
            << ",\"simulationSeconds\":" << Number(society.Time())
            << ",\"seed\":" << Quote(std::to_string(world.Seed()))
            << ",\"running\":" << (running ? "true" : "false")
            << ",\"paused\":" << (paused ? "true" : "false")
            << ",\"focused\":" << (focused ? "true" : "false")
            << ",\"blocked\":" << (error ? "true" : "false")
            << ",\"errors\":{\"world\":" << Quote(world.Error())
            << ",\"society\":" << Quote(society.Error()) << ",\"level\":" << Quote(level.Error())
            << ",\"residents\":" << Quote(residents.Error()) << '}' << ",\"summary\":{\"npcCount\":"
            << alive + residents.Residents().size() + level.Souls().size()
            << ",\"citizensAlive\":" << alive << ",\"citizensDead\":" << dead
            << ",\"residents\":" << residents.Residents().size()
            << ",\"souls\":" << level.Souls().size() << ",\"monsters\":" << monsters
            << ",\"loadedMonsters\":" << loadedMonsters
            << ",\"outsideMonsters\":" << outsideMonsters
            << ",\"monsterKinds\":" << Array(monsterKinds) << ",\"lod\":" << Array(lod)
            << ",\"chunks\":" << world.Chunks().size() << ",\"pendingChunks\":" << world.Pending()
            << '}' << ",\"player\":{\"position\":" << Position(level.Player().position)
            << ",\"hp\":" << Number(level.Player().health) << ",\"gold\":" << society.PlayerGold()
            << ",\"level\":" << level.Player().level
            << ",\"damage\":" << Number(level.Player().Damage())
            << ",\"defense\":" << Number(level.Player().gearDefense)
            << ",\"inventory\":" << Array(level.Rpg().inventory)
            << ",\"equipped\":" << Array(level.Rpg().equipped) << '}'
            << ",\"bossEvent\":{\"village\":" << level.Rpg().bossVillage
            << ",\"remainingSeconds\":" << Number(level.Rpg().bossTimer)
            << ",\"active\":" << (level.Phase() == LevelPhase::BossFight ? "true" : "false")
            << "},\"quests\":{\"claimed\":" << Array(level.Rpg().claimed)
            << ",\"hunts\":" << level.Rpg().hunts << ",\"finds\":" << level.Rpg().finds
            << ",\"trades\":" << level.Rpg().trades << ",\"bosses\":" << level.Rpg().bosses
            << "},\"actionNames\": [";
        for (int i = 0; i < int(NpcAction::Count); ++i)
        {
            if (i)
            {
                out << ',';
            }
            out << Quote(std::wstring(WorldSimulation::ActionName(NpcAction(i))));
        }
        out << "],\"villages\":[";
        for (int i = 0; i < int(villages.size()); ++i)
        {
            if (i)
            {
                out << ',';
            }
            const auto& v = villages[i];
            out << "{\"id\":" << i << ",\"name\":" << Quote(std::wstring(Villages()[i].name))
                << ",\"radius\":" << VillageWallRadius
                << ",\"position\":" << Position(Villages()[i].center)
                << ",\"homeAlive\":" << v.alive << ",\"homeDead\":" << v.dead
                << ",\"presentCitizens\":" << v.present << ",\"residents\":" << v.residents
                << ",\"souls\":" << v.souls << ",\"hostile\":" << v.hostile
                << ",\"wounded\":" << v.wounded << ",\"gold\":" << v.gold
                << ",\"goods\":" << v.goods << ",\"insight\":" << v.insight
                << ",\"averageHealthPercent\":"
                << (v.alive ? Number(100 * v.healthRatio / v.alive) : "null")
                << ",\"monsters\":" << v.monsters
                << ",\"resourceStock\":" << society.ResourceStock(i)
                << ",\"actions\":" << Array(v.actions) << '}';
        }
        out << "],\"npcs\":[" << npcJson.str() << "],\"monsters\":[" << mobJson.str()
            << "],\"news\":[";
        first = true;
        for (const auto& news : society.News())
        {
            if (!first)
            {
                out << ',';
            }
            first = false;
            out << Quote(news);
        }
        out << "]}";
        std::string payload = out.str();
        if (server_)
        {
            server_->Update(payload,
                            !running ? "stopped"
                            : error  ? "blocked"
                            : paused ? "paused"
                                     : "live");
        }
        {
            std::lock_guard<std::mutex> guard(mutex_);
            pending_ = std::move(payload); // One pending snapshot, not an unbounded history queue.
        }
        ready_.notify_one();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Monitor] Capture failed: " << e.what() << '\n';
    }
}

void WorldMonitor::WriteLoop()
{
    auto temp = path_;
    temp += L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
    bool failed = false;
    for (;;)
    {
        std::string payload;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            ready_.wait(lock,
                        [this]()
                        {
                            return stopping_ || !pending_.empty();
                        });
            if (pending_.empty() && stopping_)
            {
                return;
            }
            payload.swap(pending_);
        }
        try
        {
            std::ofstream file(temp, std::ios::binary | std::ios::trunc);
            file.write(payload.data(), std::streamsize(payload.size()));
            file.close();
            if (!file || !MoveFileExW(temp.c_str(),
                                      path_.c_str(),
                                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                if (!failed)
                {
                    std::cerr << "[Monitor] Snapshot write failed; will retry on next capture.\n";
                }
                failed = true;
            }
            else
            {
                if (failed)
                {
                    std::cout << "[Monitor] Snapshot writer recovered.\n";
                }
                failed = false;
            }
        }
        catch (const std::exception& e)
        {
            if (!failed)
            {
                std::cerr << "[Monitor] " << e.what() << '\n';
            }
            failed = true;
        }
    }
}
