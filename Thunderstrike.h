#ifndef THUNDERSTRIKE_H
#define THUNDERSTRIKE_H
#include <raylib.h>
#include <cstdint>

// One-shot lightning bolt VFX sliced from a 13-frame spritesheet (the last
// frame is blank). It strikes a fixed world point and reports a single "hit"
// on the frame the bolt connects so gameplay can apply damage then.
class Thunderstrike
{
public:
    Thunderstrike(Vector2 targetWorldCenter, Texture2D tex, Sound hitSound);
    void tick(float deltaTime);
    void Render(Vector2 knightPos);
    void setFrame(int frameIndex);
    void playImpactSound();
    int getFrame() const { return frame; }
    bool isFinished() const { return finished; }
    // true only on the single frame the bolt lands its hit
    bool justHit() const { return hitThisFrame; }
    Vector2 getTargetCenter() const { return targetCenter; }
    void setOwnerPlayerId(uint8_t id) { ownerPlayerId = id; }
    uint8_t getOwnerPlayerId() const { return ownerPlayerId; }
    void setClientPredicted(bool predicted) { clientPredicted = predicted; }
    bool isClientPredicted() const { return clientPredicted; }

private:
    Texture2D texture{};
    Sound thunderSound{};
    Vector2 targetCenter{};          // world point the bolt strikes
    float scale{4.0f};
    int frameCount{13};              // sprites in the sheet (last is blank)
    int hitFrame{2};                 // bolt connects early for responsive feel
    int frame{0};
    float runningTime{0.0f};
    float updateTime{1.0f / 24.0f};  // per-frame duration
    bool finished{false};
    bool hitThisFrame{false};        // pulses true once, on the hit frame
    bool hitApplied{false};
    uint8_t ownerPlayerId{255};
    bool clientPredicted{false};
};

#endif // THUNDERSTRIKE_H
