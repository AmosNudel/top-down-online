#include <BaseCharacter.h>
#include <TextureUtil.h>
#include <raymath.h>

BaseCharacter::BaseCharacter()
{

}

void BaseCharacter::undoMovement()
{
    worldPos = worldPosLastFrame;
}

Rectangle BaseCharacter::getCollisionRec()
{
    return Rectangle{getScreenPos().x, getScreenPos().y, scale * width, scale * height};
}

void BaseCharacter::update(float deltaTime, bool advanceAnimation)
{
    worldPosLastFrame = worldPos;

    if (advanceAnimation)
    {
        runningTime += deltaTime;
        if (runningTime >= updateTime)
        {
            frame = (frame + 1) % maxFrames;
            runningTime = 0.0f;
        }
    }

    if (Vector2LengthSqr(velocity) > 0.0f)
    {
        Vector2 direction = Vector2Normalize(velocity);
        worldPos = Vector2Add(worldPos, Vector2Scale(direction, speed));
        // only flip facing on clear horizontal motion, so near-vertical
        // movement doesn't rapidly flip the sprite back and forth
        if (direction.x < -0.25f)
            rightLeft = -1.0f;
        else if (direction.x > 0.25f)
            rightLeft = 1.0f;
        texture = run;
    }
    else
    {
        texture = idle;
    }
    velocity = {};
}

void BaseCharacter::tickCosmeticAnimation(float deltaTime, bool moving)
{
    if (moving)
    {
        texture = run;
        runningTime += deltaTime;
        if (runningTime >= updateTime)
        {
            frame = (frame + 1) % maxFrames;
            runningTime = 0.0f;
        }
    }
    else
    {
        texture = idle;
    }
}

void BaseCharacter::setAnimationFrame(int frameIndex)
{
    frame = frameIndex % maxFrames;
    runningTime = 0.0f;
}

void BaseCharacter::setTextures(Texture2D idleTex, Texture2D runTex)
{
    idle = idleTex;
    run = runTex;
    width = (float)idle.width / maxFrames;
    height = (float)idle.height;
}

Vector2 BaseCharacter::getFixedScreenOffset() const
{
    return Vector2{
        GameConfig::kGameWidth / 2.f - scale * (0.5f * width),
        GameConfig::kGameHeight / 2.f - scale * (0.5f * height)};
}

Vector2 BaseCharacter::getWorldCenter() const
{
    Vector2 offset = getFixedScreenOffset();
    return Vector2{
        worldPos.x + offset.x + scale * width * 0.5f,
        worldPos.y + offset.y + scale * height * 0.5f};
}

Rectangle BaseCharacter::getWorldCollisionRec() const
{
    Vector2 offset = getFixedScreenOffset();
    return Rectangle{
        worldPos.x + offset.x,
        worldPos.y + offset.y,
        scale * width,
        scale * height};
}

void BaseCharacter::setFacingDirection(float direction)
{
    if (direction < -0.01f)
        rightLeft = -1.0f;
    else if (direction > 0.01f)
        rightLeft = 1.0f;
}

void BaseCharacter::render()
{
    // draw the character (inset UVs avoid bleeding between atlas frames on WebGL)
    const float inset = 0.5f;
    Vector2 pos = getScreenPos();
    Rectangle source{
        frame * width + inset,
        inset,
        rightLeft * (width - inset * 2.f),
        height - inset * 2.f};
    Rectangle dest{
        SnapPixel(pos.x),
        SnapPixel(pos.y),
        scale * width,
        scale * height};
    DrawTexturePro(texture, source, dest, (Vector2){}, 0.0f, tint);
}

void BaseCharacter::tick(float deltaTime, bool simulate, bool drawSprite, bool advanceAnimation)
{
    if (simulate)
        update(deltaTime, advanceAnimation);
    if (drawSprite)
        render();
}