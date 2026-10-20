#pragma once
#include <cstdint>
#include <vector>
class LevelOne;
class WorldSimulation;
class SceneRenderer;

class RpgPanel
{
  public:
    bool IsOpen() const
    {
        return mode_ != 0;
    }

    void Close()
    {
        mode_ = 0;
        merchant_ = 0;
    }

    void Open(int mode, std::uint64_t merchant = 0);
    void Key(unsigned char key, LevelOne& level, WorldSimulation& society);
    void Click(int x, int y, int width, int height, LevelOne& level, WorldSimulation& society);
    void Draw(
        SceneRenderer& renderer, LevelOne& level, WorldSimulation& society, int width, int height);

  private:
    std::vector<int> Entries(const LevelOne& level) const;
    int mode_ = 0, selected_ = 0, page_ = 0, rows_ = 8;
    std::uint64_t merchant_ = 0;
};
