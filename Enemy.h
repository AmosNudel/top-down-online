#include <raylib.h>
#include <vector>
#include <BaseCharacter.h>
#include <Character.h>
#include <Prop.h>
#include <cstdint>

class Enemy : public BaseCharacter
{
public:
    Enemy(Vector2 pos, Texture2D idle_texture, Texture2D run_texture);
    Vector2 getWorldCenter() const;
    Rectangle getWorldCollisionRec() const;
    virtual void tick(float deltaTime, bool simulate = true, bool drawSprite = true, bool advanceAnimation = true) override;
    void setChaseTarget(Character *character) { chaseTarget = character; }
    void setCameraAnchor(Character *character) { cameraAnchor = character; }
    void setObstacles(std::vector<Prop> *props) { obstacles = props; }
    virtual Vector2 getScreenPos() const override;
    uint8_t getTargetPlayerId() const { return targetPlayerId; }
    void setTargetPlayerId(uint8_t id) { targetPlayerId = id; }
    uint8_t getEnemyType() const { return enemyType; }
    void setEnemyType(uint8_t type) { enemyType = type; }
    void setNetworkMoving(bool moving);
    void setNetworkFacing(float direction) { networkFacing = direction; }
    bool getNetworkMoving() const { return networkMoving; }
    float getDamagePerSec() const { return damagePerSec; }
    uint16_t getId() const { return id; }
    void setId(uint16_t enemyId) { id = enemyId; }

private:
    Vector2 steerAroundProps(Vector2 desiredVelocity);
    Vector2 steerAroundPropsWorld(Vector2 desiredVelocity);
    bool collidesWithProps();
    bool collidesWithPropsWorld();
    Character *chaseTarget{nullptr};
    Character *cameraAnchor{nullptr};
    std::vector<Prop> *obstacles{nullptr};
    uint8_t targetPlayerId{0};
    uint8_t enemyType{0};
    uint16_t id{0};
    bool networkMoving{false};
    float networkFacing{1.f};
    float damagePerSec{10.0f};
    float radius{25.f};
    float lookAhead{150.f};
    int avoidDir{0};
};
