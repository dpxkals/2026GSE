#include "stdafx.h"
#include "NpcSystem.h"
#include <Windows.h>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

NpcSystem::NpcSystem(const std::filesystem::path& saveDirectory)
    : savePath_(saveDirectory / L"residents_v1.dat")
{
    // Stable identities and positions in the existing origin clearing.
    // This does not regenerate or change previously saved terrain chunks.
    residents_.push_back({1,L"이안",L"봉화지기",-1.5,1.5,0,0});
    residents_.push_back({2,L"세라",L"떠도는 기록자",3.5,-2.5,1,0});
    Load();
}

bool NpcSystem::IsPresent(const Npc& npc, const World& world) const
{
    return world.CanWalk(npc.x,npc.y);
}

int NpcSystem::Nearest(double x, double y, const World& world) const
{
    int result=-1;
    double nearest=2.2*2.2;
    for (size_t i=0; i<residents_.size(); ++i)
    {
        const Npc& npc=residents_[i];
        if (!IsPresent(npc,world)) continue;
        double dx=npc.x-x, dy=npc.y-y;
        if (dx*dx+dy*dy < nearest) { nearest=dx*dx+dy*dy; result=int(i); }
    }
    return result;
}

bool NpcSystem::CanWalk(double x, double y, const World& world) const
{
    if (!world.CanWalk(x,y)) return false;
    for (const Npc& npc : residents_)
    {
        if (!IsPresent(npc,world)) continue;
        double dx=npc.x-x, dy=npc.y-y;
        if (dx*dx+dy*dy < 0.42*0.42) return false;
    }
    return true;
}

void NpcSystem::Load()
{
    try
    {
        if (!std::filesystem::exists(savePath_)) { Save(); return; }
        std::ifstream in(savePath_);
        std::string magic;
        int version=0;
        size_t count=0;
        if (!(in >> magic >> version >> count) || magic != "GSE_RESIDENTS" || version != 1 || count != residents_.size())
            throw std::runtime_error("Invalid NPC header; original file preserved");
        // Stage the entire state before accepting it.
        auto staged=residents_;
        for (Npc& npc : staged)
        {
            int id=0;
            unsigned long long meetings=0;
            if (!(in >> id >> meetings) || id != npc.id || meetings > (std::numeric_limits<unsigned int>::max)())
                throw std::runtime_error("Invalid NPC data; original file preserved");
            npc.meetings=static_cast<unsigned int>(meetings);
        }
        residents_=std::move(staged);
    }
    catch (const std::exception& e) { error_=std::string("NPC LOAD: ")+e.what(); }
}

bool NpcSystem::Save()
{
    if (!error_.empty()) return false;
    try
    {
        auto temporary=savePath_; temporary+=L".tmp";
        std::ofstream out(temporary,std::ios::trunc);
        out << "GSE_RESIDENTS 1 " << residents_.size() << '\n';
        for (const Npc& npc : residents_) out << npc.id << ' ' << npc.meetings << '\n';
        out.flush();
        if (!out) throw std::runtime_error("Cannot write NPC memory");
        out.close();
        if (!out || !MoveFileExW(temporary.c_str(),savePath_.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot commit NPC memory");
        return true;
    }
    catch (const std::exception& e) { error_=std::string("NPC SAVE: ")+e.what(); return false; }
}

std::vector<std::wstring> NpcSystem::Conversation(int index, const World& world)
{
    if (index < 0 || index >= int(residents_.size())) return {};
    Npc& npc=residents_[index];
    bool met=npc.meetings > 0;
    const Tile* beacon=world.Find(2,1);
    bool lit=beacon && beacon->prop == Prop::Beacon && beacon->lit;
    std::vector<std::wstring> pages;
    if (npc.id == 1)
    {
        pages.push_back(met
            ? L"다시 왔군. 이 길을 돌아오는 발소리는 쉽게 잊히지 않아. 무사해서 다행이야."
            : L"처음 보는 얼굴이군. 나는 이안, 이곳의 봉화를 지키고 있어. 숲에 들어가기 전에 잠시 쉬어 가.");
        pages.push_back(lit
            ? L"봉화가 다시 타오르고 있어. 누군가 돌아올 길을 남겨 둔 거지. 이 작은 불 하나로도 오늘 밤은 어제와 달라질 거야."
            : L"저 꺼진 봉화가 보이나? 가까이 가서 E를 눌러 불을 밝혀 줘. 세상을 구할 수는 없어도, 길 잃은 한 사람은 돌아오겠지.");
        pages.push_back(L"이곳에서는 죽음이 무언가를 남긴다고들 해. 재인지, 기억인지, 아니면 다른 누군가인지는… 나도 알지 못해.");
    }
    else
    {
        pages.push_back(met
            ? L"당신을 다시 기록하게 됐네요. 지난번과 같은 이름이어도, 그 사이에 걸어온 길은 다르겠죠."
            : L"세라라고 해요. 사라진 마을과 남겨진 사람들의 이야기를 적고 있죠. 당신은 무엇을 찾으며 걷고 있나요?");
        pages.push_back(lit
            ? L"봉화에 불이 들어왔군요. 오늘 기록에는 멸망 대신 그 불빛을 적겠어요. 남아 있는 것들도 기억할 가치가 있으니까요."
            : L"기록에는 이상한 빈칸이 있어요. 누군가의 마지막 날 바로 다음에, 출신을 모르는 사람의 첫날이 적혀 있죠. 우연일까요?");
        pages.push_back(L"당신이 어떤 사람이었는지, 언젠가 다른 사람의 말투에서 알아볼 수도 있겠죠. 그때는… 모른 척하지 말아 주세요.");
    }
    if (error_.empty() && npc.meetings < (std::numeric_limits<unsigned int>::max)())
    {
        ++npc.meetings;
        if (!Save()) --npc.meetings;
    }
    return pages;
}
