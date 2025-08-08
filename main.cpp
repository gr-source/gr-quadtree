#include <SDL2/SDL.h>
#include <SDL_render.h>

#include <alloca.h>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <oneapi/tbb/info.h>
#include <ostream>
#include <vector>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGTH 600

typedef struct Vector2
{
    union
    {
        struct
        {
            float x;
            float y;
        };
        float value[2];
    };
} Vector2;

typedef struct Rect
{
    union
    {
        struct
        {
            float x;
            float y;
            float w;
            float h;
        };
        float data[4];
    };

    // center-based
    bool contains(const Vector2 &point) const
    {
        return (point.x >= x - w && point.x  < x + w &&
                point.y >= y - h && point.y  < y + h);
    }
    
    /*
    // corner-based
    bool contains(const Vector2& point) const
    {
        return (point.x >= x && point.x <= x + w &&
                point.y >= y && point.y <= y + h);
    }
    */

    bool intersects(const Rect &range) const
    {
        return !(range.x - range.w > x + w ||
            range.x + range.w < x - w ||
            range.y - range.h > y + h ||
            range.y + range.h < y - h);
    }
} Rect;

using QuadtreeID = std::size_t;

constexpr QuadtreeID Null = std::numeric_limits<QuadtreeID>::max();

typedef struct Player
{
    Vector2 position;
    Vector2 velocity;

    QuadtreeID id;

    void update(float dt)
    {
        position.x += velocity.x * dt;
        position.y += velocity.y * dt;

        if (position.x < 0) {
            position.x = 0;
            velocity.x *= -1;
        }
        else if (position.x > WINDOW_WIDTH - 2) {
            position.x = WINDOW_WIDTH - 2;
            velocity.x *= -1;
        }

        if (position.y < 0) {
            position.y = 0;
            velocity.y *= -1;
        }
        else if (position.y > WINDOW_HEIGTH - 2) {
            position.y = WINDOW_HEIGTH - 2;
            velocity.y *= -1;
        }
    }
} Player;

typedef struct QuadTreeNode
{
    QuadtreeID parent;

    QuadtreeID northwest;
    QuadtreeID northeast;
    QuadtreeID southwest;
    QuadtreeID southeast;

    QuadtreeID first;
    QuadtreeID next;

    void *data[4];
    std::size_t size;

    Rect boundary;

    bool divided;
} QuadTreeNode;

template <typename T>
struct vector
{
    T *values;

    std::size_t count;
};

typedef struct fquery
{
    QuadtreeID id;
    void *data;
} fquery;

class QuadTreeManager
{
public:
    QuadTreeNode *m_nodes;

    std::size_t m_size;

    QuadtreeID m_count;

    QuadtreeID m_freeID;

    std::size_t m_capacity;

    QuadTreeManager(std::size_t size, std::size_t capacity) : m_nodes(nullptr), m_count(0), m_size(size), m_freeID(Null), m_capacity(capacity)
    {
        m_nodes = (QuadTreeNode *)std::calloc(size, sizeof(QuadTreeNode));

        addfree(0);
    }

    ~QuadTreeManager()
    {
        for (std::size_t i={};i<m_size;i++)
        {
            QuadTreeNode &node = m_nodes[i];
            // if (node.data != nullptr)
            {
                // std::free(node.data);
            }
        }
        std::free(m_nodes);
    }

    void addfree(QuadtreeID id)
    {
        for (QuadtreeID i = id; i < m_size - 1; i++)
        {
            m_nodes[i].next = i + 1;
        }

        m_nodes[m_size - 1].next = m_freeID;
        m_freeID = id;
    }

    void allocate()
    {
        std::size_t ooldsize = m_size;
        std::size_t newsize = m_size * 2;

        QuadTreeNode *array = (QuadTreeNode *)std::malloc(sizeof(QuadTreeNode) * newsize);
        if (!array)
        {
            std::cerr << "Erro: malloc falhou\n";
            std::abort();
        }

        std::memcpy(array, m_nodes, sizeof(QuadTreeNode) * ooldsize);
        std::free(m_nodes);

        m_size = newsize;

        m_nodes = array;

        addfree(ooldsize);
    }

    QuadtreeID Create(const Rect &boundary, QuadtreeID parent = Null)
    {
        if (m_freeID == Null)
        {
            allocate();
        }

        QuadtreeID id = m_freeID;
        m_freeID = m_nodes[id].next;

        QuadTreeNode &node = m_nodes[id];
        node.parent = parent;
        node.divided = false;
        node.boundary = boundary;
        node.first = Null;
        node.next = Null;
        node.size = 0;

        for (int i=0;i<m_capacity;i++)
            node.data[i] = nullptr;

        m_count++;

        return id;
    }

    void deallocate(QuadtreeID id)
    {
        QuadTreeNode &node = m_nodes[id];

        if (node.divided)
        {
            deallocate(node.northwest);
            deallocate(node.northeast);
            deallocate(node.southwest);
            deallocate(node.southeast);
        }

        node.next = m_freeID;
        m_freeID = id;
        /*
        QuadtreeID lastIndex = m_count - 1;
        if (id != lastIndex)
        {
            m_nodes[id] = m_nodes[lastIndex];

            if (m_nodes[id].parent != Null)
            {
                QuadTreeNode &parent = m_nodes[m_nodes[id].parent];

                if (parent.northwest == lastIndex)
                    parent.northwest = id;
                else if (parent.northeast == lastIndex)
                    parent.northeast = id;
                else if (parent.southwest == lastIndex)
                    parent.southwest = id;
                else if (parent.southeast == lastIndex)
                    parent.southeast = id;
            }

            if (m_nodes[id].divided)
            {
                m_nodes[m_nodes[id].northwest].parent = id;
                m_nodes[m_nodes[id].northeast].parent = id;
                m_nodes[m_nodes[id].southwest].parent = id;
                m_nodes[m_nodes[id].southeast].parent = id;
            }
        }

        m_nodes[lastIndex].next = m_freeID;
        m_freeID = lastIndex;

        std::memset(&m_nodes[lastIndex], 0, sizeof(QuadTreeNode));
        */
        m_count--;
    }

    bool Insert(QuadtreeID id, Player *player)
    {
        QuadTreeNode *node = m_nodes + id;
        if (!node->boundary.contains(player->position))
            return false;

        if (node->size < m_capacity)
        {
            auto index = node->size;

            node->data[index] = player;
            player->id = id;
            node->size++;
            return true;
        } else
        {
            if (!node->divided)
            {
                subdivide(id);

                node = m_nodes + id;
            }

            if (Insert(node->northeast, player))
                return true;
            else if (Insert(node->northwest, player))
                return true;
            else if (Insert(node->southeast, player))
                return true;
            else if (Insert(node->southwest, player))
                return true;
        }
        return false;
    }

    bool remove(QuadtreeID id, Player *player)
    {
        QuadTreeNode &node = m_nodes[id];

        for (QuadtreeID i={};i<node.size;i++)
        {
            Player *p = (Player *)node.data[i];
            if (p == player)
            {
                node.data[i] = node.data[node.size - 1];
                node.data[node.size - 1] = nullptr;
                node.size--;
                return true;
            }
        }

        if (node.divided)
        {
            if (remove(node.northwest, player))
                return check(id);
            else if (remove(node.northeast, player))
                return check(id);
            else if (remove(node.southwest, player))
                return check(id);
            else if (remove(node.southeast, player))
                return check(id);
        }

        return false;
    }

    bool check(QuadtreeID id)
    {
        QuadTreeNode &node = m_nodes[id];

        tryCollapse(node);

        return true;
    }

    void tryCollapse(QuadTreeNode &node)
    {
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
    }

    void update(QuadtreeID id, QuadtreeID root, float dt)
    {
        QuadTreeNode *node = m_nodes + id;

        std::vector<Player *> players;
        
        auto temp = node->size;
        for (QuadtreeID i={};i<temp;i++)
        {
            Player *player = (Player *)node->data[i];
            if (player == nullptr)
                continue;

            if (!node->boundary.contains(player->position))
            {
                if (remove(id, player))
                {
                    players.push_back(player);
                    // if (Insert(root, player))
                    {

                    }
                }
            }
        }

        for (auto &player : players)
            Insert(root, player);


        node = m_nodes + id;

        if (node->divided)
        {
            update(node->northeast, root, dt);
            update(node->northwest, root, dt);
            update(node->southeast, root, dt);
            update(node->southwest, root, dt);

            tryCollapse(*node);
        }
    }

    bool empty(QuadtreeID id)
    {
        QuadTreeNode &node = m_nodes[id];
        if (node.size > 0)
            return false;

        if (!node.divided)
            return true;

        return empty(node.northeast) &&
            empty(node.northwest) &&
            empty(node.southeast) &&
            empty(node.southwest);
    }

    void subdivide(QuadtreeID id)
    {
        Rect boundary = m_nodes[id].boundary;
        float x = boundary.x;
        float y = boundary.y;
        float w = boundary.w;
        float h = boundary.h;

        Rect ne = {x + w / 2, y - h / 2, w / 2, h / 2};
        QuadtreeID northeast = Create(ne, id);
        assert(northeast != Null && "Invalid Create northeast");

        Rect nw = {x - w / 2, y - h / 2, w / 2, h / 2};
        QuadtreeID northwest = Create(nw, id);
        assert(northwest != Null && "Invalid Create northwest");

        Rect se = {x + w / 2, y + h / 2, w / 2, h / 2};
        QuadtreeID southeast = Create(se, id);
        assert(southeast != Null && "Invalid Create southeast");

        Rect sw = {x - w / 2, y + h / 2, w / 2, h / 2};
        QuadtreeID southwest = Create(sw, id);
        assert(southwest != Null && "Invalid Create southwest");

        QuadTreeNode &node = m_nodes[id];
        node.northwest = northwest;
        node.northeast = northeast;
        node.southwest = southwest;
        node.southeast = southeast;
        node.divided = true;
    }

    void renderer(SDL_Renderer *renderer, Rect *bounds, QuadtreeID root)
    {
        return render(renderer, bounds, m_nodes[root]);
    }

    void render(SDL_Renderer *renderer, Rect *bounds, const QuadTreeNode &node)
    {
        Rect boundary = node.boundary;

        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_Rect rect = {
            static_cast<int>(boundary.x - boundary.w),
            static_cast<int>(boundary.y - boundary.h),
            static_cast<int>(boundary.w * 2),
            static_cast<int>(boundary.h * 2)
        };
        SDL_RenderDrawRect(renderer, &rect);

        int pointSize = 3;
            
        for (std::size_t j={};j<node.size;j++)
        {
            Player *player = (Player *)node.data[j];
            if (player == nullptr)
                continue;

            if (bounds->contains(player->position))
                SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
            else
                SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);

            SDL_Rect pRect = {
                static_cast<int>(player->position.x - static_cast<float>(pointSize) / 2),
                static_cast<int>(player->position.y - static_cast<float>(pointSize) / 2),
                pointSize,
                pointSize
            };
            SDL_RenderFillRect(renderer, &pRect);
        }

        if (node.divided)
        {
            render(renderer, bounds, m_nodes[node.northeast]);
            render(renderer, bounds, m_nodes[node.northwest]);
            render(renderer, bounds, m_nodes[node.southeast]);
            render(renderer, bounds, m_nodes[node.southwest]);
        }
    }

    bool query(QuadtreeID id, const Rect &range, std::vector<fquery> &found)
    {
        QuadTreeNode &node = m_nodes[id];
        if (!node.boundary.intersects(range))
        {
            return false;
        } else {
            for (std::size_t i={};i<node.size;i++)
            {
                Player *player = (Player *)node.data[i];

                if (range.contains(player->position))
                    found.push_back({id, (void *)player});
            }

            if (node.divided)
            {
                query(node.northwest, range, found);
                query(node.northeast, range, found);
                query(node.southwest, range, found);
                query(node.southeast, range, found);
            }
        }
        return true;
    }
};


#include <random>
int main()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "Erro ao iniciar SDL: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "SDL2 + Loop ESC para sair",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGTH,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cerr << "Erro ao criar janela: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "Erro ao criar renderer: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event event;

    QuadTreeManager s_QuadTreeManager(1024, 4);

    QuadtreeID root = s_QuadTreeManager.Create(Rect{WINDOW_WIDTH / 2.0f, WINDOW_HEIGTH / 2.0f, WINDOW_WIDTH / 2.0f, WINDOW_HEIGTH / 2.0f}, 4);

    int playercount = 10000;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> distX(0.0f, (float)WINDOW_WIDTH);
    std::uniform_real_distribution<float> distY(0.0f, (float)WINDOW_HEIGTH);

    std::vector<std::unique_ptr<Player>> playerlist; //(playercount);
    for (int i=0;i<playercount;i++)
    {
        auto &player = playerlist.emplace_back(std::make_unique<Player>());
        player->position = Vector2{distX(gen), distY(gen)};
        player->velocity = Vector2{0.6f, 0.6f};

        s_QuadTreeManager.Insert(root, player.get());
    }

    int mouseX = 0, mouseY = 0;
    Uint32 lastTime = 0;

    int downMouseX = 0, downMouseY = 0;
    bool isMouseDown = false;

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
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN)
            {
                if (event.button.button == SDL_BUTTON_RIGHT)
                {
                    downMouseX = event.button.x;
                    downMouseY = event.button.y;

                    isMouseDown = true;

                    Vector2 point = {(float)downMouseX, (float)downMouseY};


                    auto &player = playerlist.emplace_back(std::make_unique<Player>());
                    player->position = point;
                    player->velocity = Vector2{0.6f, 0.6f};

                    if (s_QuadTreeManager.Insert(root, player.get()))
                    {
                        // std::cout << "added: " << mouseX << ", " << mouseY << std::endl;
                    } else
                    {
                        /*
                        Rect boundary = s_QuadTree.get_boundary();
                        float x = std::min(boundary.x, point.x);
                        float y = std::min(boundary.y, point.y);

                        float w = std::abs(point.x - boundary.x);
                        float h = std::abs(point.y - boundary.y);

                        float diff = std::abs(w - h);

                        boundary.w = std::max(boundary.w, diff) * 1.2f;
                        boundary.h = std::max(boundary.h, diff) * 1.2f;

                        s_QuadTree.set_boundary(boundary);

                        s_QuadTree.insert(point);
                        */
                    }
                }
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    int mouseX = event.button.x;
                    int mouseY = event.button.y;

                    Rect rect = {(float)mouseX, (float)mouseY, 50, 50};

                    std::vector<fquery> found;
                    s_QuadTreeManager.query(root, rect, found);

                    for (auto &&[id, player] : found)
                    {
                        s_QuadTreeManager.remove(id, (Player *)player);
                    }
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
        float deltaTime = (currentTime - lastTime) / 10.0f;
        lastTime = currentTime;

        for (auto &player : playerlist)
        {
            player->update(deltaTime);
        }

        s_QuadTreeManager.update(root, root, deltaTime);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        /*
        for (auto &player : playerlist)
        {
            SDL_Rect rect = {(int)player.position.x - 5, (int)player.position.y - 5, 5 * 2, 5 * 2};
            SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
            SDL_RenderFillRect(renderer, &rect);
        }
        */

        Rect _rect = {(float)mouseX, (float)mouseY, 50, 50};
        s_QuadTreeManager.renderer(renderer, (Rect *)&_rect, root);
        
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

        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
