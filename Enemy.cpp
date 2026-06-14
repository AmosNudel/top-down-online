#include <Enemy.h>
#include <raymath.h>

void Enemy::setNetworkMoving(bool moving)
{
    networkMoving = moving;
    texture = moving ? run : idle;
}

Enemy::Enemy(Vector2 pos, Texture2D idle_texture, Texture2D run_texture)
{
    worldPos = pos;
    texture = idle_texture;
    idle = idle_texture;
    run = run_texture;

    width = (float)texture.width / maxFrames;
    height = (float)texture.height;
    speed = 3.5f;
}

Vector2 Enemy::getWorldCenter() const
{
    Rectangle rec = getWorldCollisionRec();
    return Vector2{rec.x + rec.width * 0.5f, rec.y + rec.height * 0.5f};
}

Rectangle Enemy::getWorldCollisionRec() const
{
    return Rectangle{worldPos.x, worldPos.y, scale * width, scale * height};
}

void Enemy::tick(float deltaTime, bool simulate, bool drawSprite, bool advanceAnimation)
{
    if (!getAlive())
        return;

    if (simulate)
    {
        if (!chaseTarget || !chaseTarget->getAlive())
            return;

        Vector2 targetCenter = chaseTarget->getWorldCenter();
        Vector2 enemyCenter = getWorldCenter();
        Vector2 diff = Vector2Subtract(targetCenter, enemyCenter);
        float dist = Vector2Length(diff);

        if (dist < radius)
        {
            velocity = {};
            networkMoving = false;
        }
        else
        {
            velocity = Vector2Scale(Vector2Normalize(diff), speed);
            velocity = steerAroundPropsWorld(velocity);
            networkMoving = true;
        }

        BaseCharacter::update(deltaTime, advanceAnimation);

        if (collidesWithPropsWorld())
        {
            Vector2 moved = worldPos;
            worldPos = Vector2{moved.x, worldPosLastFrame.y};
            if (collidesWithPropsWorld())
            {
                worldPos = Vector2{worldPosLastFrame.x, moved.y};
                if (collidesWithPropsWorld())
                    worldPos = worldPosLastFrame;
            }
        }
    }
    else if (drawSprite)
    {
        if (!cameraAnchor)
            return;

        tickCosmeticAnimation(deltaTime, networkMoving);
        setFacingDirection(networkFacing);
    }

    if (drawSprite)
        BaseCharacter::render();
}

Vector2 Enemy::steerAroundPropsWorld(Vector2 desiredVelocity)
{
    if (!obstacles || obstacles->empty())
        return desiredVelocity;

    float spd = Vector2Length(desiredVelocity);
    if (spd == 0.f)
        return desiredVelocity;

    Vector2 dir = Vector2Normalize(desiredVelocity);
    Vector2 pos = getWorldCenter();
    float enemyHalf = scale * width * 0.5f;

    bool blocked = false;
    float nearestAhead = lookAhead + 1.f;
    Vector2 threatCenter{};
    for (auto &prop : *obstacles)
    {
        Rectangle rec = prop.getWorldCollisionRec();
        Vector2 center{rec.x + rec.width / 2.f, rec.y + rec.height / 2.f};
        Vector2 toProp = Vector2Subtract(center, pos);

        float ahead = Vector2DotProduct(toProp, dir);
        if (ahead <= 0.f || ahead > lookAhead)
            continue;

        Vector2 perp = Vector2Subtract(toProp, Vector2Scale(dir, ahead));
        float perpDist = Vector2Length(perp);
        float corridor = (rec.width + rec.height) / 4.f + enemyHalf;
        if (perpDist < corridor && ahead < nearestAhead)
        {
            nearestAhead = ahead;
            threatCenter = center;
            blocked = true;
        }
    }

    if (!blocked)
    {
        avoidDir = 0;
        return desiredVelocity;
    }

    Vector2 toThreat = Vector2Subtract(threatCenter, pos);
    float cross = dir.x * toThreat.y - dir.y * toThreat.x;
    if (avoidDir == 0)
        avoidDir = (cross > 0.f) ? 1 : -1;

    Vector2 tangent = (avoidDir > 0)
                          ? Vector2{dir.y, -dir.x}
                          : Vector2{-dir.y, dir.x};

    return Vector2Scale(Vector2Normalize(tangent), spd);
}

bool Enemy::collidesWithPropsWorld()
{
    if (!obstacles)
        return false;

    for (auto &prop : *obstacles)
    {
        if (CheckCollisionRecs(getWorldCollisionRec(), prop.getWorldCollisionRec()))
            return true;
    }
    return false;
}

Vector2 Enemy::steerAroundProps(Vector2 desiredVelocity)
{
    return steerAroundPropsWorld(desiredVelocity);
}

bool Enemy::collidesWithProps()
{
    return collidesWithPropsWorld();
}

Vector2 Enemy::getScreenPos() const
{
    if (!cameraAnchor)
        return worldPos;
    return Vector2Subtract(worldPos, cameraAnchor->getWorldPos());
}
