#pragma once

#include "math.hpp"

#include "default.hpp"

struct Player
{
    Vector2 position =
        { 0.0f,   0.0f };
    Vector2 velocity =
        { 64.0f,  64.0f };

    QuadtreeID node =
        INVALID_QUADTREEID;
};

void Player_Update(Player* player, float dt);

