#pragma once

#include "math.hpp"

#include "default.hpp"

#define QUADTREE_DEPTH 8

struct Player
{
    union
    {
        struct
        {
            Vector2 position;
            Vector2 scale;
        };
        Rect bounds;
    };

    Vector2 velocity =
        { 64.0f,  64.0f };

    float speed =
        64.0f;
};

void Player_Update(Player* player, float dt);

