#include "stdafx.h"
#include "WorldSimulation.h"
#include "LevelOne.h"
#include "VillageLayout.h"
#include "GameplayBehaviors.h"
#include <algorithm>
#include <cmath>
#include <set>

const Citizen* WorldSimulation::NearbyTrader(WorldPoint player) const
{
    const Citizen* best = nullptr;
    double distance = 3.5;
    for (const auto& npc : people_)
    {
        if (!CanTrade(npc.id, player))
        {
            continue;
        }
        double d = Gameplay::Distance(player, npc.position);
        if (npc.id == selected_)
        {
            return &npc;
        }
        if (d <= distance)
        {
            best = &npc;
            distance = d;
        }
    }
    return best;
}

bool WorldSimulation::CanTrade(std::uint64_t id, WorldPoint player) const
{
    const auto* npc = Find(id);
    int town = VillageAt(player);
    return error_.empty() && npc && npc->health > 0 && npc->hostility <= 0 && town >= 0 &&
           VillageAt(npc->position) == town && Gameplay::Distance(player, npc->position) <= 3.5;
}

int WorldSimulation::ItemPrice(int item, std::uint64_t trader, bool buying) const
{
    const auto* npc = Find(trader);
    if (item < 0 || item >= RpgItemCount || !npc)
    {
        return 0;
    }
    bool merchant = Tendency(*npc) == NpcTendency::Merchant || npc->action == NpcAction::Trade ||
                    npc->learned[size_t(NpcAction::Trade)] >= 4;
    int price = ItemCatalog()[item].price;
    return buying ? (merchant ? price * 85 / 100 : price) : price * (merchant ? 40 : 30) / 100;
}

bool WorldSimulation::ExchangeItemGold(std::uint64_t trader,
                                       WorldPoint player,
                                       int item,
                                       bool buying)
{
    if (!CanTrade(trader, player) || item < 0 || item >= RpgItemCount)
    {
        return false;
    }
    auto& npc = *Find(trader);
    int price = ItemPrice(item, trader, buying);
    if (price <= 0 || (buying ? playerGold_ < price : npc.gold < price))
    {
        return false;
    }
    if ((buying ? npc.gold : playerGold_) > 1000000000 - price)
    {
        return false;
    }
    playerGold_ += buying ? -price : price;
    npc.gold += buying ? price : -price;
    Learn(npc, NpcAction::Trade, 1);
    Record(buying ? "equipment_buy" : "equipment_sell",
           0,
           trader,
           price,
           std::wstring(ItemCatalog()[item].name) + (buying ? L" 구매" : L" 판매"));
    return true;
}

void WorldSimulation::RewardGold(int amount)
{
    playerGold_ = (std::min)(1000000000, playerGold_ + amount);
}

CombatTarget WorldSimulation::PlayerTarget(WorldPoint player,
                                           double range,
                                           const World& world) const
{
    CombatTarget result;
    // Explicit selection never makes unrelated neutral citizens targetable.
    if (const auto* chosen = Find(attackTarget_))
    {
        if (chosen->health > 0 && Gameplay::Distance(player, chosen->position) <= range &&
            world.ClearLine(player.x, player.y, chosen->position.x, chosen->position.y))
        {
            return {CombatKind::Citizen, chosen->id, chosen->position, chosen->health};
        }
    }
    for (const auto& npc : people_)
    {
        double distance = Gameplay::Distance(player, npc.position);
        if (npc.health > 0 && npc.hostility > 0 && distance <= range &&
            world.ClearLine(player.x, player.y, npc.position.x, npc.position.y))
        {
            range = distance;
            result = {CombatKind::Citizen, npc.id, npc.position, npc.health};
        }
    }
    return result;
}

bool WorldSimulation::HitProjectile(Projectile& shot, WorldPoint)
{
    auto* npc = Find(shot.npcTarget);
    if (!npc || npc->health <= 0)
    {
        return false;
    }
    if (Gameplay::Distance(shot.position, npc->position) > 0.38)
    {
        return false;
    }
    Damage(*npc, shot.damage, CombatKind::Player, 0);
    return true;
}

bool WorldSimulation::HitHostileProjectile(Projectile& shot)
{
    for (auto& npc : people_)
    {
        if (npc.health > 0 && VillageAt(npc.position) < 0 &&
            Gameplay::Distance(shot.position, npc.position) <= 0.38)
        {
            Damage(npc, shot.damage, CombatKind::Monster, 0);
            return true;
        }
    }
    return false;
}

void WorldSimulation::MonsterKilled(std::uint64_t killer, const Enemy& enemy)
{
    auto* npc = Find(killer);
    if (!npc || npc->health <= 0)
    {
        return;
    }
    int reward = enemy.kind == EnemyKind::Boss ? 60 : 5;
    npc->gold += reward;
    ++npc->goods;
    Grow(*npc, enemy.kind == EnemyKind::Boss ? 100 : 12);
    npc->health = (std::min)(npc->maxHealth, npc->health + 8);
    for (auto& ally : people_)
    {
        if (ally.id != killer && ally.health > 0 && ally.action == NpcAction::Hunt &&
            ally.targetKind == CombatKind::Monster && ally.target == enemy.id &&
            Gameplay::Distance(ally.position, enemy.position) < 8)
        {
            Grow(ally, enemy.kind == EnemyKind::Boss ? 40 : 6);
            Learn(ally, NpcAction::Hunt, 1);
            ally.dirty = true;
        }
    }
    Learn(*npc, NpcAction::Hunt, 3);
    npc->dirty = true;
    Record("npc_hunt", killer, enemy.id, reward, Name(*npc) + L"가 몬스터를 사냥해 성장했다.");
}

void WorldSimulation::PlayerMonsterKilled()
{
    playerGold_ += 4;
    ++playerGoods_;
}

std::wstring WorldSimulation::LootPlayer(WorldPoint position)
{
    if (!error_.empty())
    {
        return L"저장 오류로 습득이 중단되었습니다.";
    }
    for (auto& bag : loot_)
    {
        if (bag.remaining > 0 && Gameplay::Distance(position, bag.position) < 2)
        {
            playerGold_ += bag.gold;
            playerGoods_ += bag.goods;
            bag.remaining = 0;
            Record("player_loot", 0, bag.id, bag.gold, L"플레이어가 떨어진 소지품을 챙겼다.");
            return L"소지품 습득: 금화 " + std::to_wstring(bag.gold) + L", 재료 " +
                   std::to_wstring(bag.goods);
        }
    }
    return L"근처에 습득할 소지품이 없습니다.";
}

std::wstring WorldSimulation::TradePlayer(LevelOne& level, bool training)
{
    if (!error_.empty())
    {
        return L"저장 오류로 거래가 중단되었습니다.";
    }
    auto position = level.Player().position;
    int village = VillageAt(position);
    if (village < 0)
    {
        return L"마을의 주민 가까이에서 거래하세요.";
    }
    for (auto& npc : people_)
    {
        if (!training && selected_ && CanTrade(selected_, position) && npc.id != selected_)
            continue;
        if (npc.health <= 0 || npc.hostility > 0 || VillageAt(npc.position) != village ||
            Gameplay::Distance(position, npc.position) > 3.5)
        {
            continue;
        }
        if (!training && playerGoods_ > 0)
        {
            int count = (std::min)(playerGoods_, npc.gold / 3);
            if (count <= 0)
            {
                continue;
            }
            playerGoods_ -= count;
            playerGold_ += count * 3;
            npc.gold -= count * 3;
            npc.goods += count;
            Learn(npc, NpcAction::Trade, 2);
            Record(
                "player_sell", 0, npc.id, count * 3, L"플레이어가 주민에게 사냥 재료를 판매했다.");
            return L"재료 판매: +" + std::to_wstring(count * 3) + L" 금화";
        }
        int price = training ? 20 : 10;
        if (npc.goods <= 0)
        {
            continue;
        }
        if (playerGold_ < price)
        {
            return L"금화가 부족합니다.";
        }
        if (!training && level.Player().health >= level.Player().MaxHealth())
        {
            return L"이미 체력이 가득합니다.";
        }
        if (training && level.Player().level >= MaxCharacterLevel)
        {
            return L"현재 성장 한도에 도달했습니다.";
        }
        playerGold_ -= price;
        npc.gold += price;
        --npc.goods;
        Learn(npc, NpcAction::Trade, 2);
        level.ReceiveTrade(training);
        Record("player_buy",
               0,
               npc.id,
               price,
               training ? L"플레이어가 성장 지식을 구매했다."
                        : L"플레이어가 회복 물품을 구매했다.");
        return training ? L"20 금화: 경험치 20 획득" : L"10 금화: 체력 35 회복";
    }
    return L"3.5타일 이내에 재고·금화를 가진 거래 가능한 주민이 없습니다.";
}

void WorldSimulation::SynchronizeActors(SceneGraph& scene, const World& world)
{
    auto& root = scene.Ensure("society", "group");
    std::set<std::string> keys;
    for (const auto& npc : people_)
    {
        if (npc.health <= 0 ||
            !world.Find(WorldInt(std::floor(npc.position.x)), WorldInt(std::floor(npc.position.y))))
        {
            continue;
        }
        auto key = "society/npc/" + std::to_string(npc.id);
        keys.insert(key);
        if (!scene.Find(key))
        {
            auto id = npc.id;
            auto& actor = scene.Ensure(key, "citizen", root.GetId());
            actor.recordId = id;
            actor.BindPosition(
                [this, id]()
                {
                    const auto* citizen = Find(id);
                    return citizen ? ActorPosition{citizen->position.x, citizen->position.y}
                                   : ActorPosition{};
                },
                [this, id](ActorPosition position)
                {
                    if (auto* citizen = Find(id))
                    {
                        citizen->position = {position.x, position.y};
                    }
                });
        }
    }
    scene.Retain("society/npc/", keys);
    keys.clear();
    for (const auto& bag : loot_)
    {
        if (bag.remaining <= 0 ||
            !world.Find(WorldInt(std::floor(bag.position.x)), WorldInt(std::floor(bag.position.y))))
        {
            continue;
        }
        auto key = "society/loot/" + std::to_string(bag.id);
        keys.insert(key);
        auto& actor = scene.Ensure(key, "citizen_loot", root.GetId());
        actor.recordId = bag.id;
        scene.SetPosition(actor.GetId(), {bag.position.x, bag.position.y});
    }
    scene.Retain("society/loot/", keys);
    scene.Ensure("society/strikes", "social_strikes", root.GetId()).layer = RenderLayer::Effects;
}
