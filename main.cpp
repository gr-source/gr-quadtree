#include <SDL2/SDL.h>
#include <SDL_render.h>

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <vector>

#include "math.hpp"
#include "player.hpp"

#include "sparseSet.hpp"

#include <iostream>

#define MAX_PLAYERS 0

static constexpr size_t MAX_DEPTH = 8;
static constexpr size_t CAPACITY = 4;

struct QuadTreeNode
{
    QuadtreeID parent =
        INVALID_QUADTREEID;

    QuadtreeID northwest =
        INVALID_QUADTREEID;
    QuadtreeID northeast =
        INVALID_QUADTREEID;
    QuadtreeID southwest =
        INVALID_QUADTREEID;
    QuadtreeID southeast =
        INVALID_QUADTREEID;

    PlayerID data[CAPACITY];
    size_t count = 0;

    Rect boundary;

    bool isLeaf() const
    {
        return northwest == INVALID_QUADTREEID
            || northeast == INVALID_QUADTREEID
            || southwest == INVALID_QUADTREEID
            || southeast == INVALID_QUADTREEID;
    }   
};

struct fquery
{
    QuadtreeID id;
    void *data;
};

class QuadTreeManager
{
public:
    ~QuadTreeManager()
    {
        if (m_freeList != nullptr)
            free(m_freeList);

        if (m_nodes != nullptr)
            free(m_nodes);
    }

    QuadtreeID Create(const Rect &boundary, QuadtreeID parent = INVALID_QUADTREEID)
    {
        if (m_freeID == INVALID_QUADTREEID)
            grow();

        QuadtreeID id = m_freeID;
        m_freeID = m_freeList[id];

        QuadTreeNode* node = m_nodes + id;
        new (node) QuadTreeNode();

        node->parent = parent;
        node->boundary = boundary;

        m_count++;

        return id;
    }

    QuadtreeID Insert(QuadtreeID id, const Rect& bounds, PlayerID playerID)
    {
        QuadTreeNode *node = m_nodes + id;
        if (!node->boundary.intersects(bounds))
            return INVALID_QUADTREEID;

        if (node->count < CAPACITY)
        {
            node->data[node->count++] = playerID;
            return id;
        } else
        {
            if (node->isLeaf())
            {
                subdivide(id);

                node = m_nodes + id;
            }

            QuadtreeID quadtreeID =
                INVALID_QUADTREEID;

            quadtreeID = Insert(node->northwest, bounds, playerID);
            if (quadtreeID != INVALID_QUADTREEID)
                return quadtreeID;
            quadtreeID = Insert(node->northeast, bounds, playerID);
            if (quadtreeID != INVALID_QUADTREEID)
                return quadtreeID;

            quadtreeID = Insert(node->southwest, bounds, playerID);
            if (quadtreeID != INVALID_QUADTREEID)
                return quadtreeID;
            quadtreeID = Insert(node->southeast, bounds, playerID);
            if (quadtreeID != INVALID_QUADTREEID)
                return quadtreeID;
        }
        return INVALID_QUADTREEID;
    }

    bool Sync(QuadtreeID id, const Vector2& point, PlayerID playerID)
    {
        assert(m_capacity > id);

        QuadTreeNode& node =
            m_nodes[id];

        if (node.boundary.contains(point))
            return false;

        for (size_t i=0;i<node.count;i++)
        {
            if (node.data[i] == playerID)
            {
                size_t last = node.count - 1;
                if (i != last)
                {
                    node.data[i] = node.data[last];
                }
                node.count--;
                break;
            }
        }

        if (!node.count)
        {
        }

        return true;
    }

    void renderer(SDL_Renderer *context, QuadtreeID root)
    {
        QuadTreeNode& node = m_nodes[root];

        Rect boundary = node.boundary;

        SDL_SetRenderDrawColor(context, 255, 0, 0, 255);
        SDL_FRect rect =
        {
            .x = boundary.x,
            .y = boundary.y,
            .w = boundary.w,
            .h = boundary.h
        };
        SDL_RenderDrawRectF(context, &rect);

        if (!node.isLeaf())
        {
            renderer(context, node.northwest);
            renderer(context, node.northeast);
            renderer(context, node.southwest);
            renderer(context, node.southeast);
        }
    }

    bool query(QuadtreeID id, const Rect &range, std::vector<fquery> &found)
    {
        return false;
        /*
        QuadTreeNode &node = m_nodes[id];
        if (!node.boundary.intersects(range))
        {
            return false;
        } else {
            for (std::size_t i={};i<node.count;i++)
            {
                Player *player = (Player *)node.data[i];

                if (range.contains(player->position))
                    found.push_back({id, (void *)player});
            }

            if (node.divided)
            {
                for (QuadtreeID i=node.first;i!=INVALID_QUADTREEID;i=m_nodes[i].next)
                    query(i, range, found);
            }
        }
        return true;
        */
    }

private:
    QuadTreeNode *m_nodes =
        nullptr;

    size_t m_capacity =
        0;

    size_t m_count = 0;

    QuadtreeID* m_freeList =
        nullptr;

    QuadtreeID m_freeID =
        INVALID_QUADTREEID;

    void subdivide(QuadtreeID id)
    {
        Rect boundary = m_nodes[id].boundary;

        float halfWidth  = boundary.w / 2.0f;
        float halfHeight = boundary.h / 2.0f;

        float x = boundary.x;
        float y = boundary.y;

        // Top Left
        QuadtreeID northwest =
            Create(
                {
                    x,
                    y,
                    halfWidth,
                    halfHeight
                },
                id
            );
        assert(northwest != INVALID_QUADTREEID && "Invalid Create northwest");

        // Top Right
        QuadtreeID northeast =
            Create(
                {
                    x + halfWidth,
                    y,
                    halfWidth,
                    halfHeight
                },
                id
            );
        assert(northeast != INVALID_QUADTREEID && "Invalid Create northeast");

        // Bottom Left
        QuadtreeID southwest =
            Create(
                {
                    x,
                    y + halfHeight,
                    halfWidth,
                    halfHeight
                },
                id
            );
        assert(southwest != INVALID_QUADTREEID && "Invalid Create southwest");

        // Bottom Right
        QuadtreeID southeast =
            Create(
                {
                    x + halfWidth,
                    y + halfHeight,
                    halfWidth,
                    halfHeight
                },
                id
            );
        assert(southeast != INVALID_QUADTREEID && "Invalid Create southeast");

        QuadTreeNode &node = m_nodes[id];
        node.northwest = northwest;
        node.northeast = northeast;
        node.southwest = southwest;
        node.southeast = southeast;
    }

    void grow()
    {
        size_t oldCapacity = m_capacity;

        m_capacity = m_capacity > 0 ? m_capacity * 2 : 2;

        m_nodes =
            (QuadTreeNode *)realloc(m_nodes, sizeof(QuadTreeNode) * m_capacity);

        m_freeList =
            (QuadtreeID*)realloc(m_freeList, sizeof(QuadtreeID) * m_capacity);

        addFreeList(oldCapacity);
    }

    void addFreeList(size_t begin)
    {
        size_t end =
            m_capacity - 1;

        for (size_t i = begin; i < end; i++)
            m_freeList[i] =
                static_cast<QuadtreeID>(i + 1);

        m_freeList[end] = m_freeID;
        m_freeID = static_cast<QuadtreeID>(begin);
    }
};

void Player_Renderer(Player* player, SDL_Renderer* context)
{
    Vector2 position = { player->bounds.x, player->bounds.y };
    Vector2 scale = { player->bounds.w, player->bounds.h };

    const SDL_FRect rect =
    {
        .x = position.x - scale.x * 0.5f,
        .y = position.y - scale.x * 0.5f,
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

    QuadTreeManager s_QuadTreeManager;

    QuadtreeID root =
        s_QuadTreeManager.Create(
            {
                0.0f,
                0.0f,
                WINDOW_WIDTH,
                WINDOW_HEIGTH
            }
        );

    SparseSet2<Player> playerList;
    
    // std::vector<std::unique_ptr<Player>> playerlist;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> distX(0.0f, (float)WINDOW_WIDTH);
    std::uniform_real_distribution<float> distY(0.0f, (float)WINDOW_HEIGTH);

    for (size_t i=0;i<MAX_PLAYERS;i++)
    {
        Rect bounds =
        {
            distX(gen),
            distY(gen),
            16.0f,
            16.0f
        };

        PlayerID playerID =
            playerList.emplace(Player{
                .bounds = bounds,
                .velocity = { 64.0f, 64.0f }
            });

        QuadtreeID node =
            s_QuadTreeManager.Insert(root, bounds, playerID);

        auto& player =
            playerList.get(playerID);
        player.node = node;
    }
    // */

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

                    /*
                    Rect rect = {(float)mouseX, (float)mouseY, 50, 50};

                    std::vector<fquery> found;
                    s_QuadTreeManager.query(root, rect, found);

                    for (auto &&[id, player] : found)
                    {
                        s_QuadTreeManager.remove(root, (Player *)player);
                    }
                    */
                }
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN)
            {
                if (event.button.button == SDL_BUTTON_RIGHT)
                {
                    downMouseX = event.button.x;
                    downMouseY = event.button.y;

                    isMouseDown = true;

                    Rect bounds =
                    {
                        (float)downMouseX,
                        (float)downMouseY,
                        16.0f,
                        16.0f
                    };

                    PlayerID playerID =
                        playerList.emplace(Player{
                            .bounds = bounds,
                            .velocity = { 64.0f, 64.0f }
                        });

                    QuadtreeID node =
                        s_QuadTreeManager.Insert(root, bounds, playerID);

                    auto& player =
                        playerList.get(playerID);
                    player.node = node;
                }
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    int mouseX = event.button.x;
                    int mouseY = event.button.y;

                    /*
                    Rect rect = {(float)mouseX, (float)mouseY, 50, 50};

                    std::vector<fquery> found;
                    s_QuadTreeManager.query(root, rect, found);

                    for (auto &&[id, player] : found)
                    {
                        s_QuadTreeManager.remove(id, (Player *)player);
                    }
                    */
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

        for (size_t i=0;i<playerList.count();i++)
            Player_Update(players + i, deltaTime);

        for (size_t i=0;i<playerList.count();i++)
        {
            Player& player =
                players[i];

            PlayerID playerID =
                playerList.denseToSparse(i);

            // if (s_QuadTreeManager.Sync(player.node, player.position, playerID))
            // {
            //     player.node =
            //         s_QuadTreeManager.Insert(root, player.position, playerID);
            // }
        }

        // s_QuadTreeManager.update(root, root, deltaTime);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        for (size_t i=0;i<playerList.count();i++)
            Player_Renderer(players + i, renderer);

        // Rect _rect = {(float)mouseX, (float)mouseY, 50, 50};
        s_QuadTreeManager.renderer(renderer, root);
        
        SDL_Rect rect = {mouseX - 50, mouseY - 50, 50 * 2, 50 * 2};
        SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
        SDL_RenderDrawRect(renderer, &rect);


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
