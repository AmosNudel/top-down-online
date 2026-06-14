#ifndef PICKUP_H
#define PICKUP_H
#include <raylib.h>

class Character;

enum class PickupType
{
    HEALTH
};

class Pickup
{
public:
    Pickup(Vector2 pos, Texture2D sheet, Rectangle source, PickupType type);

    void Render(Vector2 knightPos) const;
    Rectangle getCollisionRec(Vector2 knightPos);
    Rectangle getWorldCollisionRec();
    void apply(Character &knight);
    bool isActive() const { return active; }
    void collect(Character &knight);
    Vector2 getWorldCenter() const;

    static constexpr float kScale = 2.0f;

private:
    Texture2D texture{};
    Rectangle source{};
    Vector2 worldPos{};
    float scale{kScale};
    PickupType type{PickupType::HEALTH};
    bool active{true};
};

#endif // PICKUP_H
