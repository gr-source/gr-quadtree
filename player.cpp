#include "player.hpp"

void Player_Update(Player* player, float dt)
{
    player->position.x += player->velocity.x * dt;
    player->position.y += player->velocity.y * dt;

    if (player->position.x - player->scale.x< 0)
    {
        // player->position.x = 0;
        player->velocity.x *= -1;
    }
    else if (player->position.x + player->scale.x > WINDOW_WIDTH - 2)
    {
        // player->position.x = WINDOW_WIDTH - 2;
        player->velocity.x *= -1;
    }

    if (player->position.y - player->scale.x< 0)
    {
        // player->position.y = 0;
        player->velocity.y *= -1;
    }
    else if (player->position.y + player->scale.y > WINDOW_HEIGTH - 2)
    {
        // player->position.y = WINDOW_HEIGTH - 2;
        player->velocity.y *= -1;
    }
}

