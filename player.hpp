#pragma once

#include "math.hpp"

#include "default.hpp"

struct Player
{
    Rect bounds;

    Vector2 velocity =
        { 64.0f,  64.0f };

    float speed =
        64.0f;

    QuadtreeID node =
        INVALID_QUADTREEID;
};

void Player_Update(Player* player, float dt);

