#pragma once

#include "math.hpp"

#include "default.hpp"

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

    Quadtree<PlayerID, 8>* node =
        nullptr;
};

void Player_Update(Player* player, float dt);

