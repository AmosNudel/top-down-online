#include <prop.h>
#include <raymath.h>

Prop::Prop(Vector2 pos, Texture2D tex):
    worldPos{pos}, 
    texture{tex}
{

}

void Prop::Render(Vector2 knightPos) const
{
    Vector2 screenPos{Vector2Subtract(worldPos, knightPos)};
    DrawTextureEx(texture, screenPos, 0.0f, scale, WHITE);
}

Rectangle Prop::getCollisionRec(Vector2 knightPos)
{
    Vector2 screenPos{Vector2Subtract(worldPos, knightPos)};
    return Rectangle{screenPos.x, screenPos.y, texture.width * scale, texture.height * scale};
}

Rectangle Prop::getPlayerCollisionRec(Vector2 knightPos)
{
    Rectangle full = getCollisionRec(knightPos);
    // inset prop bounds for the player only; enemies keep the full rectangle
    float insetX = full.width * 0.22f;
    float insetY = full.height * 0.28f;
    return Rectangle{
        full.x + insetX,
        full.y + insetY,
        full.width - insetX * 2.f,
        full.height - insetY * 2.f};
}

Rectangle Prop::getWorldCollisionRec() const
{
    return Rectangle{worldPos.x, worldPos.y, texture.width * scale, texture.height * scale};
}