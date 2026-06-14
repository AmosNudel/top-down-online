#ifndef PROP_H
#define PROP_H
#include <raylib.h>

class Prop
{
public:
    Prop(Vector2 pos, Texture2D tex);
    void Render(Vector2 knightPos) const;
    Rectangle getCollisionRec(Vector2 knightPos);
    Rectangle getPlayerCollisionRec(Vector2 knightPos);
    Rectangle getWorldCollisionRec() const;

private:
    Texture2D texture{};
    Vector2 worldPos{};
    float scale{4.0f};
};

#endif // PROP_H