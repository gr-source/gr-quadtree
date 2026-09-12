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

#define MAX_NODE_ITENS 10

struct QuadTreeNode
{
    QuadtreeID parent =
        INVALID_QUADTREEID;

    QuadtreeID first =
        INVALID_QUADTREEID;
    QuadtreeID next =
        INVALID_QUADTREEID;

    PlayerID data[MAX_NODE_ITENS];
    size_t count = 0;

    Rect boundary;

    bool divided = false;
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

    void deallocate(QuadtreeID id)
    {
        QuadTreeNode &node = m_nodes[id];

        if (node.divided)
        {
            for (QuadtreeID i=node.first;i!=INVALID_QUADTREEID;)
            {
                QuadtreeID next = m_nodes[i].next;
                deallocate(i);
                i = next;
            }
            /*
            deallocate(node.northwest);
            deallocate(node.northeast);
            deallocate(node.southwest);
            deallocate(node.southeast);
            */
        }

        if (node.parent != INVALID_QUADTREEID)
        {
            QuadTreeNode &parent = m_nodes[node.parent];
            if (parent.first == id)
                parent.first = node.next;
            else
                parent.next = node.next;
        }

        m_freeList[id] = m_freeID;
        m_freeID = id;

        /*
        node.first = INVALID_QUADTREEID;
        node.next = m_freeID;
        m_freeID = id;
        */

        m_count--;
    }

    QuadtreeID Insert(QuadtreeID id, const Vector2& point, PlayerID playerID)
    {
        QuadTreeNode *node = m_nodes + id;
        if (!node->boundary.contains(point))
            return INVALID_QUADTREEID;

        if (node->count < MAX_NODE_ITENS)
        {
            node->data[node->count++] = playerID;
            // player->id = id;
            return id;
        } else
        {
            if (!node->divided)
            {
                subdivide(id);

                node = m_nodes + id;
            }

            for (QuadtreeID i=node->first;i!=INVALID_QUADTREEID;i=m_nodes[i].next)
            {
                QuadtreeID result =
                    Insert(i, point, playerID);
                if (result != INVALID_QUADTREEID)
                    return result;
            }
            /*
            if (Insert(node->northeast, player, depth + 1))
                return true;
            if (Insert(node->northwest, player, depth + 1))
                return true;
            if (Insert(node->southeast, player, depth + 1))
                return true;
            if (Insert(node->southwest, player, depth + 1))
                return true;
            */
        }
        return INVALID_QUADTREEID;
    }

    bool remove(QuadtreeID id, PlayerID playerID)
    {
        QuadTreeNode &node = m_nodes[id];
        
        for (size_t i={};i<node.count;i++)
        {
            PlayerID index =
                node.data[i];
            if (index == playerID)
            {
                node.data[i] = node.data[node.count - 1];
                node.data[node.count - 1] = INVALID_PLAYERID;

                node.count--;
                return true;
            }
        }

        if (node.divided)
        {
            for (QuadtreeID i=node.first;i!=INVALID_QUADTREEID;i=m_nodes[i].next)
            {
                if (remove(i, playerID))
                    return check(id);
            }
            /*
            if (remove(node.northwest, player))
                return check(node);
            if (remove(node.northeast, player))
                return check(node);
            if (remove(node.southwest, player))
                return check(node);
            if (remove(node.southeast, player))
                return check(node);
            */
        }

        return false;
    }

    bool check(QuadtreeID id)
    {
        tryCollapse(id);

        return true;
    }

    void tryCollapse(QuadtreeID id)
    {
        QuadTreeNode &node = m_nodes[id];
        if (!node.divided)
            return;

        for (QuadtreeID i=node.first;i!=INVALID_QUADTREEID;i=m_nodes[i].next)
        {
            if (!empty(i))
                return;
        }

        for (QuadtreeID i=node.first;i!=INVALID_QUADTREEID;)
        {
            QuadtreeID next = m_nodes[i].next;
            deallocate(i);
            i = next;
        }

        node.divided = false;

        /*
        if (node.divided &&
            empty(node.northeast) &&
            empty(node.northwest) &&
            empty(node.southeast) &&
            empty(node.southwest))
        {
            deallocate(node.northeast);
            deallocate(node.northwest);
            deallocate(node.southeast);
            deallocate(node.southwest);

            node.divided = false;
        }
        */
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

    bool empty(QuadtreeID id)
    {
        QuadTreeNode &node = m_nodes[id];
        if (node.count > 0)
            return false;

        if (!node.divided)
            return true;

        return true;
        /*
        return empty(node.northeast) &&
            empty(node.northwest) &&
            empty(node.southeast) &&
            empty(node.southwest);
        */
    }

    void emplace(QuadtreeID id, QuadtreeID child)
    {
        QuadTreeNode &node = m_nodes[id];
        if (node.first == INVALID_QUADTREEID)
        {
            node.first = child;
        } else
        {
            QuadtreeID sibling = node.first;
            while (m_nodes[sibling].next != INVALID_QUADTREEID)
                sibling = m_nodes[sibling].next;

            m_nodes[sibling].next = child;
        }
    }

    void renderer(SDL_Renderer *context, QuadtreeID root)
    {
        QuadTreeNode& node = m_nodes[root];

        Rect boundary = node.boundary;

        SDL_SetRenderDrawColor(context, 255, 0, 0, 255);
        SDL_Rect rect = {
            static_cast<int>(boundary.x - boundary.w),
            static_cast<int>(boundary.y - boundary.h),
            static_cast<int>(boundary.w * 2),
            static_cast<int>(boundary.h * 2)
        };
        SDL_RenderDrawRect(context, &rect);

        if (node.divided)
        {
            for (QuadtreeID id =node.first;id!=INVALID_QUADTREEID;id=m_nodes[id].next)
                renderer(context, id);
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
        float x = boundary.x;
        float y = boundary.y;
        float w = boundary.w;
        float h = boundary.h;

        Rect ne = {x + w / 2, y - h / 2, w / 2, h / 2};
        QuadtreeID northeast = Create(ne, id);
        assert(northeast != INVALID_QUADTREEID && "Invalid Create northeast");

        Rect nw = {x - w / 2, y - h / 2, w / 2, h / 2};
        QuadtreeID northwest = Create(nw, id);
        assert(northwest != INVALID_QUADTREEID && "Invalid Create northwest");

        Rect se = {x + w / 2, y + h / 2, w / 2, h / 2};
        QuadtreeID southeast = Create(se, id);
        assert(southeast != INVALID_QUADTREEID && "Invalid Create southeast");

        Rect sw = {x - w / 2, y + h / 2, w / 2, h / 2};
        QuadtreeID southwest = Create(sw, id);
        assert(southwest != INVALID_QUADTREEID && "Invalid Create southwest");

        QuadTreeNode &node = m_nodes[id];
        emplace(id, northwest);
        emplace(id, northeast);
        emplace(id, southwest);
        emplace(id, southeast);
        /*
        node.northwest = northwest;
        node.northeast = northeast;
        node.southwest = southwest;
        node.southeast = southeast;
        */
        node.divided = true;
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
    SDL_Rect rect = {(int)player->position.x - 5, (int)player->position.y - 5, 5 * 2, 5 * 2};

    SDL_SetRenderDrawColor(context, 255, 0, 255, 255);
    SDL_RenderFillRect(context, &rect);
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

    QuadtreeID root = s_QuadTreeManager.Create(Rect{WINDOW_WIDTH / 2.0f, WINDOW_HEIGTH / 2.0f, WINDOW_WIDTH / 2.0f, WINDOW_HEIGTH / 2.0f}, 4);

    SparseSet2<Player> playerList;
    
    // std::vector<std::unique_ptr<Player>> playerlist;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> distX(0.0f, (float)WINDOW_WIDTH);
    std::uniform_real_distribution<float> distY(0.0f, (float)WINDOW_HEIGTH);

    for (size_t i=0;i<MAX_PLAYERS;i++)
    {
        Vector2 position =
            { distX(gen), distY(gen) };

        PlayerID playerID =
            playerList.emplace(Player{
                .position = position,
                .velocity = { 64.0f, 64.0f }
            });

        QuadtreeID node =
            s_QuadTreeManager.Insert(root, position, playerID);

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

                    Vector2 position = {(float)downMouseX, (float)downMouseY};

                    PlayerID playerID =
                        playerList.emplace(Player{
                            .position = position,
                            .velocity = { 64.0f, 64.0f }
                        });

                    QuadtreeID node =
                        s_QuadTreeManager.Insert(root, position, playerID);

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

            if (s_QuadTreeManager.Sync(player.node, player.position, playerID))
            {
                player.node =
                    s_QuadTreeManager.Insert(root, player.position, playerID);
            }
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
