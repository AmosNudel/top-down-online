#ifndef BASE_CHARACTER_H
#define BASE_CHARACTER_H
#include <raylib.h>
#include "GameConfig.h"

class BaseCharacter
{
public:
    BaseCharacter();
    Vector2 getWorldPos() const { return worldPos; }
    void setWorldPos(Vector2 pos) { worldPos = pos; worldPosLastFrame = pos; }
    void undoMovement();
    Rectangle getCollisionRec();
    void update(float deltaTime, bool advanceAnimation = true);
    void render();
    virtual void tick(float deltaTime, bool simulate = true, bool drawSprite = true, bool advanceAnimation = true);
    virtual Vector2 getScreenPos() const = 0;
    Vector2 getFixedScreenOffset() const;
    Vector2 getWorldCenter() const;
    Rectangle getWorldCollisionRec() const;
    void setFacingDirection(float direction);
    float getFacingDirection() const { return rightLeft; }
    bool getAlive() const { return alive; }
    void setAlive(bool isAlive) { alive = isAlive; }
    void setTint(Color color) { tint = color; }
    Color getTint() const { return tint; }
    void setWorldPosInterpolated(Vector2 pos) { worldPos = pos; }
    void tickCosmeticAnimation(float deltaTime, bool moving);
    int getAnimationFrame() const { return frame; }
    void setAnimationFrame(int frameIndex);
    void setTextures(Texture2D idleTex, Texture2D runTex);
protected:
    Texture2D texture{};
    Texture2D idle{};
    Texture2D run{};
    Vector2 worldPos{};
    Vector2 worldPosLastFrame{};
    // 1 : facing right, -1 : left
    float rightLeft{1.f};
    // animation variables
    float runningTime{};
    int frame{};
    int maxFrames{6};
    float updateTime{1.0f / 12.0f};
    float speed{6.0f};
    float width{};
    float height{};
    float scale{4.0f};
    Vector2 velocity{};
    Color tint{WHITE};
private:
    bool alive{true};
};

#endif // BASE_CHARACTER_H