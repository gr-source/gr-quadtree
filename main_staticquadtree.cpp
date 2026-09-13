#include <SDL2/SDL.h>
#include <SDL_render.h>

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <vector>

#include "math.hpp"
#include "player.hpp"

#include "sparseSet.hpp"

#include <iostream>

#define MAX_PLAYERS 0
#define PLAYER_SIZE 8.0f

#define MOUSE_QUAD_SIZE 50.0f

template <typename T, size_t MAX_DEPTH>
class Quadtree
{
public:
    Quadtree(const Rect& bounds, int depth) : m_boundary(bounds), m_depth(depth)
    {
        float halfWidth  = bounds.w / 2.0f;
        float halfHeight = bounds.h / 2.0f;

        float x = bounds.x;
        float y = bounds.y;

        // Top Left
        m_childBoundary[0] =
        {
            x,
            y,
            halfWidth,
            halfHeight
        };

        // Top Right
        m_childBoundary[1] =
        {
            x + halfWidth,
            y,
            halfWidth,
            halfHeight
        };

        // Bottom Left
        m_childBoundary[2] =
        {
            x,
            y + halfHeight,
            halfWidth,
            halfHeight
        };

        // Bottom Right
        m_childBoundary[3] =
        {
            x + halfWidth,
            y + halfHeight,
            halfWidth,
            halfHeight
        };
    }

    Quadtree* insert(const Rect& bounds, T object)
    {
        if (m_depth + 1 < MAX_DEPTH)
        {
            for (size_t i=0;i<4;i++)
            {
                if (m_childBoundary[i].contains(bounds))
                {
                    if (isLeaf())
                        subdivide();

                    return m_children[i]->insert(bounds, object);
                }
            }
        }

        m_objects.push_back({.bounds = bounds, .element = object });

        return this;
    }

    void query(const Rect& bounds, std::vector<T>& objects) const
    {
        if (!m_boundary.intersects(bounds))
            return;

        for (const auto& object : m_objects)
        {
            if (bounds.intersects(object.bounds))
                objects.push_back(object.element);
        }

        if (!isLeaf())
        {
            for (size_t i = 0; i < 4; ++i)
            {
                if (m_childBoundary[i].intersects(bounds))
                    m_children[i]->query(bounds, objects);
            }
        }
    }

    bool contains(const Rect& bounds) const
    {
        return m_boundary.contains(bounds);
    }

    void draw(SDL_Renderer* context) const
    {
        SDL_SetRenderDrawColor(context, 255, 0, 0, 255);
        SDL_FRect rect =
        {
            .x = m_boundary.x,
            .y = m_boundary.y,
            .w = m_boundary.w,
            .h = m_boundary.h
        };
        SDL_RenderDrawRectF(context, &rect);

        for (auto&& [bounds, playerID] : m_objects)
        {
            Vector2 position = { bounds.x, bounds.y };
            Vector2 scale = { bounds.w, bounds.h };

            const SDL_FRect rect =
            {
                .x = position.x,
                .y = position.y,
                .w = scale.x,
                .h = scale.y
            };

            SDL_SetRenderDrawColor(context, 255, 0, 255, 255);
            SDL_RenderFillRectF(context, &rect);
        }

        if (!isLeaf())
        {
            for (size_t i=0;i<4;i++)
                m_children[i]->draw(context);
        }
    }

private:
    struct ObjectWrapper
    {
        Rect bounds;
        T element;
    };

    std::unique_ptr<Quadtree<T, MAX_DEPTH>> m_children[4];

    std::vector<ObjectWrapper> m_objects;

    Rect m_childBoundary[4];

    Rect m_boundary;

    int m_depth;

    void subdivide()
    {
        for (int i=0;i<4;i++)
            m_children[i] = std::make_unique<Quadtree<T, MAX_DEPTH>>(m_childBoundary[i], m_depth + 1);
    }

    bool isLeaf() const
    {
        return
            m_children[0] == nullptr &&
            m_children[1] == nullptr &&
            m_children[2] == nullptr &&
            m_children[3] == nullptr;
    }
};

void Player_Renderer(Player* player, SDL_Renderer* context)
{
    Vector2 position = { player->bounds.x, player->bounds.y };
    Vector2 scale = { player->bounds.w, player->bounds.h };

    const SDL_FRect rect =
    {
        .x = position.x,
        .y = position.y,
        .w = scale.x,
        .h = scale.y
    };

    SDL_SetRenderDrawColor(context, 255, 0, 255, 255);
    SDL_RenderFillRectF(context, &rect);
}

#include <random>
int main()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return 1;

    SDL_Window* window = SDL_CreateWindow(
        "gr-quadtree",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGTH,
        SDL_WINDOW_SHOWN
    );

    if (window == nullptr)
    {
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr)
    {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GL_SetSwapInterval(1);

    bool running = true;
    SDL_Event event;

    const Vector2 mainScale =
        { WINDOW_WIDTH, WINDOW_HEIGTH};
        // { WINDOW_WIDTH / 2.0f, WINDOW_HEIGTH / 2.0f };

    const Vector2 mainPosition =
        { 0.0f, 0.0f };
        // { WINDOW_WIDTH / 2.0f - mainScale.x * 0.5f, WINDOW_HEIGTH / 2.0f - mainScale.y * 0.5f };

    Quadtree<PlayerID, 8> root(
            {
                mainPosition.x,
                mainPosition.y,
                mainScale.x,
                mainScale.y
            },
            0
        );

    SparseSet2<Player> playerList;
    
    // std::vector<std::unique_ptr<Player>> playerlist;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> distX(0.0f, (float)WINDOW_WIDTH - PLAYER_SIZE);
    std::uniform_real_distribution<float> distY(0.0f, (float)WINDOW_HEIGTH - PLAYER_SIZE);

    for (size_t i=0;i<MAX_PLAYERS;i++)
    {
        Rect bounds =
        {
            distX(gen),
            distY(gen),
            PLAYER_SIZE,
            PLAYER_SIZE
        };

        PlayerID playerID =
            playerList.emplace(Player{
                .bounds = bounds,
                .velocity = { 32.0f, 32.0f }
            });

        Quadtree<PlayerID, 8>* node =
            root.insert(bounds, playerID);

        auto& player =
            playerList.get(playerID);
        player.node = node;
    }

    int mouseX = 0, mouseY = 0;
    Uint32 lastTime = 0;

    int downMouseX = 0, downMouseY = 0;
    bool isMouseDown = false;

    float fpsTimeCount = 0;

    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = false;
            } else if (event.type == SDL_KEYDOWN)
            {
                if (event.key.keysym.sym == SDLK_ESCAPE)
                {
                    running = false;
                }

            }
            else if (event.type == SDL_MOUSEMOTION)
            {
                mouseX = event.button.x;
                mouseY = event.button.y;

                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    int mouseX = event.button.x;
                    int mouseY = event.button.y;
                }
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN)
            {
                if (event.button.button == SDL_BUTTON_RIGHT)
                {
                    downMouseX = event.button.x;
                    downMouseY = event.button.y;

                    isMouseDown = true;

                    const Rect bounds =
                    {
                        (float)downMouseX - PLAYER_SIZE * 0.5f,
                        (float)downMouseY - PLAYER_SIZE * 0.5f,
                        PLAYER_SIZE,
                        PLAYER_SIZE
                    };

                    PlayerID playerID =
                        playerList.emplace(Player{
                            .bounds = bounds,
                            .velocity = { 64.0f, 64.0f }
                        });

                    Quadtree<PlayerID, 8>* node =
                        root.insert(bounds, playerID);

                    auto& player =
                        playerList.get(playerID);
                    player.node = node;
                }
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    int mouseX = event.button.x;
                    int mouseY = event.button.y;

                    const Rect rect =
                        {
                            mouseX - MOUSE_QUAD_SIZE * 0.5f,
                            mouseY - MOUSE_QUAD_SIZE * 0.5f,
                            MOUSE_QUAD_SIZE,
                            MOUSE_QUAD_SIZE
                        };

                    std::vector<PlayerID> players;
                    root.query(rect, players);

                    std::cout << players.size() << std::endl;
                }
            } 
            else if (event.type == SDL_MOUSEBUTTONUP)
            {
                if (event.button.button == SDL_BUTTON_RIGHT)
                {
                    isMouseDown = false;
                }
            }
        }

        Uint32 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

        fpsTimeCount += deltaTime;
        if (fpsTimeCount > 1.0f)
        {
            fpsTimeCount -= 1.0f;

            std::cout << "FPS: " << (1.0f / deltaTime) << std::endl;
        }

        Player* players =
            playerList.data();

        /*
        for (size_t i=0;i<playerList.count();i++)
        {
            Player& player =
                players[i];

            Player_Update(&player, deltaTime);

            PlayerID playerID =
                playerList.denseToSparse(i);

            if (player.node != nullptr)
            {
                if (!player.node->contains(player.bounds))
                {
                    player.node->remove(playerID);
                    player.node = root.insert(player.bounds, playerID);

                    std::cout << "Nd\n";
                }
            }
        }
        */

        // s_QuadTreeManager.update(root, root, deltaTime);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        /*
        for (size_t i=0;i<playerList.count();i++)
            Player_Renderer(players + i, renderer);
        */

        // Rect _rect = {(float)mouseX, (float)mouseY, 50, 50};
        root.draw(renderer);
        
        SDL_FRect rect =
            {
                mouseX - MOUSE_QUAD_SIZE * 0.5f,
                mouseY - MOUSE_QUAD_SIZE * 0.5f,
                MOUSE_QUAD_SIZE,
                MOUSE_QUAD_SIZE
            };
        SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
        SDL_RenderDrawRectF(renderer, &rect);


        /*
        if (isMouseDown)
        {
            int x = std::min(downMouseX, mouseX);
            int y = std::min(downMouseY, mouseY);
            int w = std::abs(mouseX - downMouseX);
            int h = std::abs(mouseY - downMouseY);

            SDL_Rect rect = {x, y, w, h};
            SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
            SDL_RenderDrawRect(renderer, &rect);
        }
        */

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
