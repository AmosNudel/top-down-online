#ifndef CHARACTER_H
#define CHARACTER_H

#include <raylib.h>
#include <BaseCharacter.h>

class Character : public BaseCharacter
{
public:
    Character(int winWidth, int winHeight);
    virtual void tick(float deltaTime, bool simulate = true, bool drawSprite = true, bool advanceAnimation = true) override;
    virtual Vector2 getScreenPos() const override;
    Rectangle getWeaponCollisionRec() { return weaponCollisionRec; }
    Rectangle getWorldWeaponCollisionRec() const;
    // tighter feet-only hitbox used for prop collision (not enemies)
    Rectangle getPropCollisionRec();
    float getHealth() const { return health; }
    void setHealth(float value);
    void takeDamage(float damage);
    void heal(float amount);
    void playHealFeedback();
    void playHitFeedback();
    void reset();

    // sword charge: lands enough sword hits and the blade glows, unlocking thunder
    void registerSwordHit();
    bool isCharged() const { return charged; }
    void consumeCharge();
    void setCharged(bool value) { charged = value; }
    void setVirtualInput(Vector2 moveDir, bool attackPressed, bool attackHeld);
    void setPlayerColor(Color color);
    void setNetworkVisualState(bool moving, bool attackHeld);
    void setNetworkFacing(float direction) { networkFacing = direction; }
    void setCameraAnchor(const Character *anchor) { cameraAnchor = anchor; }
    void setBlockMouseAttack(bool block) { blockMouseAttack = block; }
    void prepareWeaponHitTest(bool attackHeld);
private:
    int windowWidth{};
    int windowHeight{};
    const Character *cameraAnchor{nullptr};
    Texture2D weapon{LoadTexture("characters/weapon_sword.png")};
    Rectangle weaponCollisionRec{};
    float health{100.0f};
    // red damage flash: blinks while recently hurt
    float damageFlash{0.0f};      // time remaining in the "recently hurt" window
    float flashHold{0.4f};        // how long the flash lasts after the last hit
    float flashInterval{0.08f};   // half-period of the red/normal blink
    float flashTimer{0.0f};       // drives the blink phase

    // sound effects (each played with a small random pitch offset)
    Sound attackSound{LoadSound("sfx/56_Attack_03.wav")};
    Sound stepSound{LoadSound("sfx/03_Step_grass_03.wav")};
    Sound hitSound{LoadSound("sfx/61_Hit_03.wav")};
    Sound chargeSound{LoadSound("sfx/45_Charge_05.wav")};
    Sound healSound{LoadSound("sfx/02_Heal_02.wav")};
    float stepTimer{0.0f};        // counts down to the next footstep
    float stepInterval{0.30f};    // cadence between footsteps while moving
    float hitSoundTimer{0.0f};    // gates hurt sounds so they don't spam
    float hitSoundInterval{0.7f}; // min gap between hurt sounds

    // sword charge state
    int swordHits{0};             // enemies hit since the last charge
    int hitsToCharge{20};         // sword hits needed to charge
    bool charged{false};          // true while the blade is yellow / thunder ready

    Vector2 weaponOrigin{};
    Vector2 weaponOffset{};
    float weaponRotation{};

    Vector2 virtualMove{};
    bool virtualAttackPressed{false};
    bool virtualAttackHeld{false};
    bool blockMouseAttack{false};
    bool networkMoving{false};
    bool networkAttackHeld{false};
    float networkFacing{0.f};
    Color playerColor{WHITE};

    void updateWeaponPose(bool attackHeld);
    void drawWeapon();
};

#endif // CHARACTER_H