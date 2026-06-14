#include <Pickup.h>
#include <Character.h>
#include <raymath.h>

Pickup::Pickup(Vector2 pos, Texture2D sheet, Rectangle source, PickupType type) :
    texture{sheet},
    source{source},
    worldPos{pos},
    type{type}
{
}

void Pickup::Render(Vector2 knightPos) const
{
    if (!active)
        return;

    Vector2 screenPos = Vector2Subtract(worldPos, knightPos);
    Rectangle dest{
        screenPos.x, screenPos.y,
        source.width * scale, source.height * scale};
    DrawTexturePro(texture, source, dest, Vector2{}, 0.0f, WHITE);
}

Rectangle Pickup::getCollisionRec(Vector2 knightPos)
{
    Vector2 screenPos = Vector2Subtract(worldPos, knightPos);
    return Rectangle{
        screenPos.x, screenPos.y,
        source.width * scale, source.height * scale};
}

Rectangle Pickup::getWorldCollisionRec()
{
    return Rectangle{
        worldPos.x, worldPos.y,
        source.width * scale, source.height * scale};
}

void Pickup::apply(Character &knight)
{
    switch (type)
    {
    case PickupType::HEALTH:
        knight.heal(20.f);
        break;
    }
}

void Pickup::collect(Character &knight)
{
    if (!active)
        return;
    apply(knight);
    active = false;
}

Vector2 Pickup::getWorldCenter() const
{
    return Vector2{
        worldPos.x + source.width * scale / 2.f,
        worldPos.y + source.height * scale / 2.f};
}
