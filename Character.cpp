#include <Character.h>
#include <TextureUtil.h>
#include <raymath.h>

// Play a sound with a small random pitch variation so repeated plays
// (footsteps, swings, hits) don't sound mechanically identical.
static void PlayWithRandomPitch(Sound sound, float minPitch, float maxPitch)
{
    float t = GetRandomValue(0, 1000) / 1000.0f;
    SetSoundPitch(sound, minPitch + (maxPitch - minPitch) * t);
    PlaySound(sound);
}

Character::Character(int winWidth, int winHeight) : 
    windowWidth{winWidth}, 
    windowHeight{winHeight}
{
    idle = LoadTexture("characters/knight_idle_spritesheet.png");
    run = LoadTexture("characters/knight_run_spritesheet.png");
    ConfigurePixelArtTexture(idle);
    ConfigurePixelArtTexture(run);
    ConfigurePixelArtTexture(weapon);
    texture = idle;
    width = (float)texture.width / maxFrames;
    height = (float)texture.height;
}

Vector2 Character::getScreenPos() const
{
    Vector2 screenCenter = getFixedScreenOffset();

    if (!cameraAnchor)
        return screenCenter;

    return Vector2Add(screenCenter, Vector2Subtract(worldPos, cameraAnchor->getWorldPos()));
}

Rectangle Character::getWorldWeaponCollisionRec() const
{
    Vector2 off = getFixedScreenOffset();
    return Rectangle{
        worldPos.x + weaponCollisionRec.x - off.x,
        worldPos.y + weaponCollisionRec.y - off.y,
        weaponCollisionRec.width,
        weaponCollisionRec.height};
}

void Character::setPlayerColor(Color color)
{
    playerColor = color;
    tint = color;
}

void Character::setNetworkVisualState(bool moving, bool attackHeld)
{
    networkMoving = moving;
    networkAttackHeld = attackHeld;
}

void Character::prepareWeaponHitTest(bool attackHeld)
{
    updateWeaponPose(attackHeld);
}

void Character::setVirtualInput(Vector2 moveDir, bool attackPressed, bool attackHeld)
{
    virtualMove = moveDir;
    virtualAttackPressed = attackPressed;
    virtualAttackHeld = attackHeld;
}

void Character::tick(float deltaTime, bool simulate, bool drawSprite, bool advanceAnimation)
{
    if (!getAlive())
        return;

    if (simulate)
    {
        if (Vector2LengthSqr(virtualMove) > 0.f)
        {
            velocity.x += virtualMove.x;
            velocity.y += virtualMove.y;
        }
        else
        {
            if (IsKeyDown(KEY_A))
            velocity.x -= 1.0f;
            if (IsKeyDown(KEY_D))
            velocity.x += 1.0f;
            if (IsKeyDown(KEY_W))
            velocity.y -= 1.0f;
            if (IsKeyDown(KEY_S))
            velocity.y += 1.0f;
        }

        const bool touchInput = GetTouchPointCount() > 0;
        const bool attackPressed = virtualAttackPressed ||
            (!touchInput && !blockMouseAttack && IsMouseButtonPressed(MOUSE_LEFT_BUTTON));
        const bool attackHeld = virtualAttackHeld ||
            (!touchInput && !blockMouseAttack && IsMouseButtonDown(MOUSE_LEFT_BUTTON));

        if (attackPressed)
            PlayWithRandomPitch(attackSound, 0.9f, 1.1f);

        if (Vector2Length(velocity) != 0.0f)
        {
            stepTimer -= deltaTime;
            if (stepTimer <= 0.0f)
            {
                PlayWithRandomPitch(stepSound, 0.85f, 1.15f);
                stepTimer = stepInterval;
            }
        }
        else
        {
            stepTimer = 0.0f;
        }

        if (hitSoundTimer > 0.0f)
            hitSoundTimer -= deltaTime;

        if (damageFlash > 0.0f)
        {
            damageFlash -= deltaTime;
            flashTimer += deltaTime;
            tint = (fmodf(flashTimer, flashInterval * 2.0f) < flashInterval) ? RED : WHITE;
        }
        else
        {
            flashTimer = 0.0f;
            tint = playerColor;
        }

        BaseCharacter::update(deltaTime, advanceAnimation);
        updateWeaponPose(attackHeld);
    }
    else
    {
        tint = playerColor;
        tickCosmeticAnimation(deltaTime, networkMoving);
        updateWeaponPose(networkAttackHeld);
        setFacingDirection(networkFacing);
    }

    if (drawSprite)
    {
        BaseCharacter::render();
        drawWeapon();
    }
}

void Character::updateWeaponPose(bool attackHeld)
{
    const float hitW = scale * weapon.width * 0.50f;
    const float hitH = scale * weapon.height * 0.55f;

    if (rightLeft > 0.0f)
    {
        weaponOrigin = Vector2{0.0f, weapon.height * scale};
        weaponOffset = Vector2{35.0f, 55.0f};
        weaponCollisionRec = {
            getScreenPos().x + weaponOffset.x,
            getScreenPos().y + weaponOffset.y - hitH,
            hitW,
            hitH};
        weaponRotation = attackHeld ? 35.0f : 0.0f;
    }
    else
    {
        weaponOrigin = Vector2{weapon.width * scale, weapon.height * scale};
        weaponOffset = Vector2{25.0f, 55.0f};
        weaponCollisionRec = {
            getScreenPos().x + weaponOffset.x - hitW,
            getScreenPos().y + weaponOffset.y - hitH,
            hitW,
            hitH};
        weaponRotation = attackHeld ? -35.0f : 0.0f;
    }
}

void Character::drawWeapon()
{
    const float inset = 0.5f;
    Vector2 pos = getScreenPos();
    Rectangle source{
        inset,
        inset,
        (static_cast<float>(weapon.width) - inset * 2.f) * rightLeft,
        static_cast<float>(weapon.height) - inset * 2.f};
    Rectangle dest{
        SnapPixel(pos.x + weaponOffset.x),
        SnapPixel(pos.y + weaponOffset.y),
        scale * weapon.width,
        scale * weapon.height};
    DrawTexturePro(weapon, source, dest, weaponOrigin, weaponRotation, charged ? YELLOW : WHITE);
}

Rectangle Character::getPropCollisionRec()
{
    Rectangle full = getCollisionRec();
    // narrow box at the knight's feet so gaps between props feel passable
    float insetX = full.width * 0.32f;
    float topTrim = full.height * 0.50f;
    float bottomTrim = full.height * 0.08f;
    return Rectangle{
        full.x + insetX,
        full.y + topTrim,
        full.width - insetX * 2.f,
        full.height - topTrim - bottomTrim};
}

void Character::registerSwordHit()
{
    if (charged)
        return; // already charged; hits don't stack until it's spent
    if (++swordHits >= hitsToCharge)
    {
        charged = true;
        swordHits = 0;
        PlayWithRandomPitch(chargeSound, 0.95f, 1.05f);
    }
}

void Character::consumeCharge()
{
    charged = false;
    swordHits = 0;
}

void Character::setHealth(float value)
{
    health = value;
    if (health > 100.f)
        health = 100.f;
    if (health < 0.f)
        health = 0.f;
}

void Character::takeDamage(float damage)
{
    health -= damage;
    playHitFeedback();

    if (health < 0.0f)
        setAlive(false);
}

void Character::playHitFeedback()
{
    damageFlash = flashHold;

    if (hitSoundTimer <= 0.0f)
    {
        PlayWithRandomPitch(hitSound, 0.9f, 1.1f);
        hitSoundTimer = hitSoundInterval;
    }
}

void Character::heal(float amount)
{
    health += amount;
    if (health > 100.0f)
        health = 100.0f;
    playHealFeedback();
}

void Character::playHealFeedback()
{
    PlayWithRandomPitch(healSound, 0.95f, 1.05f);
}

void Character::reset()
{
    health = 100.0f;
    setAlive(true);
    worldPos = {};
    worldPosLastFrame = {};
    velocity = {};
    damageFlash = 0.0f;
    flashTimer = 0.0f;
    stepTimer = 0.0f;
    hitSoundTimer = 0.0f;
    swordHits = 0;
    charged = false;
    tint = playerColor;
}
