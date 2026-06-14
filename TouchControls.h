#ifndef TOUCH_CONTROLS_H
#define TOUCH_CONTROLS_H

#include <raylib.h>
#include <raymath.h>

// Floating joystick (left half of screen) + action buttons on the right.
// The joystick finger is excluded from button hits so dragging past ATK won't fire attacks.
struct VirtualTouchControls
{
    Vector2 moveDir{};
    bool attackPressed{false};
    bool attackHeld{false};
    bool thunderPressed{false};
    bool thunderHeld{false};

    Vector2 defaultCenter{};
    Vector2 joystickCenter{};
    float joystickRadius{52.f};
    float knobRadius{20.f};
    Vector2 knobPos{};
    Rectangle attackBtn{};
    Rectangle thunderBtn{};
    float leftZoneMaxX{312.f};

    int joystickTouchId{-1};
    bool prevAttackHeld{false};
    bool prevThunderHeld{false};
    bool mouseOnThunderBtn{false};

    void init(float gameWidth, float gameHeight)
    {
        defaultCenter = Vector2{82.f, gameHeight - 82.f};
        joystickCenter = defaultCenter;
        knobPos = defaultCenter;
        leftZoneMaxX = gameWidth * 0.52f;
        attackBtn = Rectangle{gameWidth - 88.f, gameHeight - 88.f, 72.f, 72.f};
        thunderBtn = Rectangle{gameWidth - 88.f, gameHeight - 168.f, 72.f, 72.f};
    }

    static Vector2 MapScreenToGame(Vector2 screenPos, Rectangle viewport, float gameWidth, float gameHeight)
    {
        return Vector2{
            (screenPos.x - viewport.x) / viewport.width * gameWidth,
            (screenPos.y - viewport.y) / viewport.height * gameHeight};
    }

    static bool PointInRect(Vector2 point, Rectangle rect)
    {
        return CheckCollisionPointRec(point, rect);
    }

    void updateJoystickKnob(Vector2 point)
    {
        Vector2 delta = Vector2Subtract(point, joystickCenter);
        float dist = Vector2Length(delta);
        if (dist > joystickRadius)
        {
            knobPos = Vector2Add(joystickCenter, Vector2Scale(Vector2Normalize(delta), joystickRadius));
            moveDir = Vector2Normalize(delta);
        }
        else if (dist > 6.f)
        {
            knobPos = point;
            moveDir = Vector2Scale(delta, 1.f / joystickRadius);
        }
        else
        {
            knobPos = joystickCenter;
            moveDir = Vector2{};
        }
    }

    void beginJoystick(int touchId, Vector2 anchor)
    {
        joystickTouchId = touchId;
        joystickCenter = anchor;
        knobPos = anchor;
        moveDir = Vector2{};
    }

    void resetJoystick()
    {
        joystickTouchId = -1;
        joystickCenter = defaultCenter;
        knobPos = defaultCenter;
        moveDir = Vector2{};
    }

    // True when mouse is on BOLT — left click should thunder, not sword swing.
    bool blocksMouseAttack() const
    {
        return mouseOnThunderBtn;
    }

    bool isOnActionButton(Vector2 gamePos) const
    {
        return PointInRect(gamePos, attackBtn) || PointInRect(gamePos, thunderBtn);
    }

    bool canStartJoystickAt(Vector2 gamePos) const
    {
        return gamePos.x < leftZoneMaxX && !isOnActionButton(gamePos);
    }

    void update(Rectangle viewport, float gameWidth, float gameHeight)
    {
        attackPressed = false;
        thunderPressed = false;
        attackHeld = false;
        thunderHeld = false;
        mouseOnThunderBtn = false;

        bool joystickActive = false;
        const int touchCount = GetTouchPointCount();

        for (int i = 0; i < touchCount; i++)
        {
            Vector2 screenPos = GetTouchPosition(i);
            if (!CheckCollisionPointRec(screenPos, viewport))
                continue;

            Vector2 gamePos = MapScreenToGame(screenPos, viewport, gameWidth, gameHeight);
            const int touchId = GetTouchPointId(i);

            // Joystick finger keeps control even if it moves over buttons or off-screen zone
            if (joystickTouchId == touchId)
            {
                updateJoystickKnob(gamePos);
                joystickActive = true;
                continue;
            }

            if (PointInRect(gamePos, attackBtn))
                attackHeld = true;
            if (PointInRect(gamePos, thunderBtn))
                thunderHeld = true;

            if (joystickTouchId < 0 && canStartJoystickAt(gamePos))
            {
                beginJoystick(touchId, gamePos);
                updateJoystickKnob(gamePos);
                joystickActive = true;
            }
        }

        if (joystickTouchId >= 0)
        {
            bool fingerStillDown = false;
            for (int i = 0; i < touchCount; i++)
            {
                if (GetTouchPointId(i) == joystickTouchId)
                {
                    fingerStillDown = true;
                    break;
                }
            }
            if (!fingerStillDown)
                resetJoystick();
            else
                joystickActive = true;
        }

        // Mouse: left-click ATK / BOLT only (no virtual joystick — use WASD to move)
        if (touchCount == 0 && CheckCollisionPointRec(GetMousePosition(), viewport))
        {
            Vector2 gamePos = MapScreenToGame(GetMousePosition(), viewport, gameWidth, gameHeight);
            mouseOnThunderBtn = PointInRect(gamePos, thunderBtn);
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
            {
                if (PointInRect(gamePos, attackBtn))
                    attackHeld = true;
                if (PointInRect(gamePos, thunderBtn))
                    thunderHeld = true;
            }
        }

        if (!joystickActive && joystickTouchId < 0)
        {
            joystickCenter = defaultCenter;
            knobPos = defaultCenter;
            moveDir = Vector2{};
        }

        attackPressed = attackHeld && !prevAttackHeld;
        thunderPressed = thunderHeld && !prevThunderHeld;
        prevAttackHeld = attackHeld;
        prevThunderHeld = thunderHeld;
    }

    static void DrawActionButton(Rectangle bounds, const char *label, Color fill, bool active)
    {
        Color bg = active ? fill : Fade(fill, 0.35f);
        DrawRectangleRec(bounds, bg);
        DrawRectangleLinesEx(bounds, 2.f, RAYWHITE);
        const int fontSize{18};
        int textWidth = MeasureText(label, fontSize);
        DrawText(label,
                 (int)(bounds.x + (bounds.width - textWidth) / 2.f),
                 (int)(bounds.y + (bounds.height - fontSize) / 2.f),
                 fontSize, RAYWHITE);
    }

    void draw(bool thunderReady) const
    {
        const bool active = joystickTouchId >= 0;
        Color baseFill = active ? Fade(WHITE, 0.15f) : Fade(WHITE, 0.06f);
        Color baseLine = active ? Fade(WHITE, 0.5f) : Fade(WHITE, 0.2f);

        DrawCircle(joystickCenter.x, joystickCenter.y, joystickRadius, baseFill);
        DrawCircleLines((int)joystickCenter.x, (int)joystickCenter.y, joystickRadius, baseLine);
        if (active)
            DrawCircle(knobPos.x, knobPos.y, knobRadius, Fade(SKYBLUE, 0.7f));

        DrawActionButton(attackBtn, "ATK", MAROON, attackHeld);
        DrawActionButton(thunderBtn, "BOLT", thunderReady ? GOLD : DARKGRAY, thunderReady && thunderHeld);
    }
};

#endif
