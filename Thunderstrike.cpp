#include <Thunderstrike.h>
#include <raymath.h>

static void PlayWithRandomPitch(Sound sound, float minPitch, float maxPitch)
{
    float t = GetRandomValue(0, 1000) / 1000.0f;
    SetSoundPitch(sound, minPitch + (maxPitch - minPitch) * t);
    PlaySound(sound);
}

Thunderstrike::Thunderstrike(Vector2 targetWorldCenter, Texture2D tex, Sound hitSound) :
    texture{tex},
    thunderSound{hitSound},
    targetCenter{targetWorldCenter}
{
}

void Thunderstrike::tick(float deltaTime)
{
    hitThisFrame = false;
    if (finished)
        return;

    runningTime += deltaTime;
    if (runningTime >= updateTime)
    {
        runningTime = 0.0f;
        frame++;
        if (frame >= frameCount)
        {
            finished = true;
            return;
        }
    }

    // the bolt connects on the hit frame; report it exactly once
    if (!hitApplied && frame >= hitFrame)
    {
        hitThisFrame = true;
        playImpactSound();
    }
}

void Thunderstrike::setFrame(int frameIndex)
{
    frame = frameIndex;
    finished = frame >= frameCount;
    hitThisFrame = false;
    if (frame >= hitFrame && !hitApplied)
        playImpactSound();
    else if (frame < hitFrame)
        hitApplied = false;
}

void Thunderstrike::playImpactSound()
{
    if (hitApplied)
        return;
    hitApplied = true;
    PlayWithRandomPitch(thunderSound, 0.9f, 1.1f);
}

void Thunderstrike::Render(Vector2 knightPos)
{
    if (finished)
        return;

    float frameWidth = (float)texture.width / frameCount;
    float frameHeight = (float)texture.height;

    Rectangle source{frame * frameWidth, 0.0f, frameWidth, frameHeight};

    // anchor the bolt so its bottom-centre lands on the target point
    Vector2 screenCenter = Vector2Subtract(targetCenter, knightPos);
    Rectangle dest{
        screenCenter.x - frameWidth * scale / 2.0f,
        screenCenter.y - frameHeight * scale,
        frameWidth * scale,
        frameHeight * scale};

    DrawTexturePro(texture, source, dest, Vector2{}, 0.0f, WHITE);
}
