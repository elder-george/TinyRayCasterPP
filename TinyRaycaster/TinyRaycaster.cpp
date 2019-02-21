// TinyRaycaster.cpp : Defines the entry point for the application.
//

#include "TinyRaycaster.h"

#include <SFML/Graphics.hpp>

#include <vector>
#include <array>
#include <algorithm>
#include <cassert>
#define _USE_MATH_DEFINES 
#include <cmath>
#include <chrono>
#include <sstream>

using namespace std;

constexpr unsigned int W = 1024;
constexpr unsigned int H = 512;

static const char _map[]{ 
    "0000222222220000"\
    "1              5"\
    "1              5"\
    "1     01111    5"\
    "0     0        5"\
    "0     3     1155"\
    "0   1000       5"\
    "0   3  0       5"\
    "5   4  100011  5"\
    "5   4   1      4"\
    "0       1      4"\
    "2       1  44444"\
    "0     000      4"\
    "0 111          4"\
    "0              4"\
    "0002222244444444" 
};

struct Map {
    size_t w, h; // overall map dimensions
    Map() 
        : w(16), h(16)
    {
        assert(sizeof(_map) == w * h + 1);
    }
    int get(const size_t x, const size_t y) const
    {
        assert(y < h && x < w && "coordinate is outside of map");
        return _map[y*w + x] - '0';
    }
     bool is_empty(const size_t x, const size_t y) const {
        assert(y < h && x < w && "coordinate is outside of map");
        return _map[y*w + x] == ' ';
    }
};


using pxl = uint32_t;
inline pxl color(uint8_t r, uint8_t g, uint8_t b)
{
    return r | (g << 8) | (b << 16) | (255 << 24);
}

struct Texture {
    // instead of creating a dynamic collection of pixels, we'll return a pair of iterators
    struct iter; struct sentinel;
    using column = std::pair<typename Texture::iter, typename Texture::sentinel>;

    const sf::Image& img;
    short size, count;

    inline pxl get_pixel(size_t texture_id, size_t x, size_t y) const
    {
        auto pxlColor = img.getPixel((unsigned)(x + size * texture_id), (unsigned)y);
        auto pxl = color(pxlColor.r, pxlColor.g, pxlColor.b);
        return pxl;
    }

    inline column get_scaled_column(size_t textureId, size_t x, size_t height) const
    {
        assert(x < size && textureId < count);
        return std::make_pair(iter{*this, textureId, x, 0u, height}, sentinel{});
    }

    struct iter {
        const Texture& tex;
        size_t textureId, x, y;
        size_t height;

        inline pxl operator*() {
            return tex.get_pixel(textureId, x, y*tex.size/height);
        }

        inline iter& operator++() {
            ++y;
            return *this;
        }

        inline bool operator != (const sentinel&) {
            return y < height;
        }
    };
    // fake `end` iterator to force the bounds check for `y`.
    struct sentinel {};

};

auto begin(const Texture::column& c) -> Texture::iter {
    return get<0>(c);
}
auto end(const Texture::column& c) -> Texture::sentinel {
    return get<1>(c);
}

struct Player {
    float x, y;
    float a;
    float fov;
    int turn, walk;
};

struct GameState {
    Map map;
    Player player;
    //std::vector<Sprite> monsters;
    Texture walls;
    //Texture tex_monst;
};

struct FrameBuffer {
    FrameBuffer() {
        pixels.resize(W*H);
    }

    inline pxl& at(size_t x, size_t y) {
        assert(x < W && y < H);
        return pixels[y*W + x];
    }

    void clear(pxl p) {
        std::fill(pixels.begin(), pixels.end(), p);
    }

    void drawRectangle(size_t x, size_t y, size_t w, size_t h, pxl pixel)
    {
        for (auto i = 0u; i < h; ++i)
        {
            auto cy = y + i;
            if (cy >= H) break;
            for (auto j = 0u; j < w; ++j)
            {
                auto cx = x + j;
                if (cx >= W) break;
                at(cx, cy) = pixel;
            }
        }
    }

    void drawColumn(const Texture::column& column, size_t x, size_t y) {
        auto ptr = pixels.begin() + y * W + x;
        for (const auto pixel : column)
        {
            if (y > H)
                break;
            *ptr = pixel;
            ptr += W;
            ++y;
        }
    }

    void drawTo(sf::Texture& texture) {
        texture.update(reinterpret_cast<uint8_t*>(pixels.data()), W, H, 0, 0);
    }

private:
    std::vector<pxl> pixels;
};

inline float frac(float v) {
    return v - std::nearbyint(v);
}

inline size_t texture_x(float hit_x, float hit_y, const Texture& walls)
{
    auto x = frac(hit_x);
    auto y = frac(hit_y);
    int tex = (std::abs(y) > std::abs(x))
        ? size_t(y * walls.size)
        : size_t(x * walls.size);
    if (tex < 0)
        tex += walls.size;
    assert(tex >= 0 && tex < walls.size);
    return (size_t)tex;
}

void draw_map(FrameBuffer &fb, const Texture &walls, const Map &map, const size_t cell_w, const size_t cell_h) {
    for (size_t j=0; j<map.h; j++) {  // draw the map itself
        for (size_t i=0; i<map.w; i++) {
            if (map.is_empty(i, j)) continue; // skip empty spaces
            size_t rect_x = i*cell_w;
            size_t rect_y = j*cell_h;
            size_t texid = map.get(i, j);
            assert(texid<walls.count);
            fb.drawRectangle(rect_x, rect_y, cell_w, cell_h, walls.get_pixel(0, 0, texid)); // the color is taken from the upper left pixel of the texture #texid
        }
    }
    //for (size_t i=0; i<sprites.size(); i++) { // show the monsters
    //    fb.drawRectangle(sprites[i].x*cell_w-3, sprites[i].y*cell_h-3, 6, 6, color(255, 0, 0));
    //}
}

void render(const GameState& gs, FrameBuffer& fb)
{
    const auto& map = gs.map;
    const auto& player = gs.player;
    const auto& walls = gs.walls;
    fb.clear(color(255, 255, 255));

    auto cell_w = W / (map.w*2);
    auto cell_h = H / (map.h);

    std::array<float, W / 2> depth_buffer{ };
    depth_buffer.fill(1e3);

    for (size_t i = 0; i < W / 2; ++i)
    {
        float angle = player.a + player.fov * (i / (W / 2.f) - 0.5f);
        auto cos_a = std::cos(angle);
        auto sin_a = std::sin(angle);

        auto cos_rel = std::cos(angle - player.a);
        //Ray marching
        const float max_ray = 20;
        for (auto t = 0.f; t < max_ray; t += 0.01f)
        {
            // x has to be in [0, W / cell_w - 1] = [0, map.w*2 - 1 ]
            // 0 <= player.x + t*cos(a) < map.w * 2 - 1
            // player.x >= max_ray && player.x < map.w*2 - max_ray - 1
            float x = player.x + t * cos_a;
            assert(x >= 0 && (size_t)x <= map.w - 1);
            assert(size_t(x) * cell_w < W);
            // y needs to be in [0, H / cell_h) = [0, map.h)
            float y = player.y + t * sin_a;
            assert(y >= 0 && (size_t)y <= map.h - 1);
            assert(size_t(y) * cell_h < H);
            
            // visibility cone
            fb.at((size_t)(x * cell_w), (size_t)(y*cell_h)) = color(190, 190, 190);

            auto map_x = (size_t)x;
            auto map_y = (size_t)y;
            if (map.is_empty(map_x, map_y)) continue;

            auto tex_id = map.get(map_x, map_y); // our ray touches a wall, so draw the vertical column to create an illusion of 3D
            assert(tex_id >= 0 && tex_id < walls.count);
            
            float dist = t * cos_rel;
            depth_buffer[i] = dist;
            size_t column_h = std::min<size_t>(2000, size_t(H / dist));

            auto tex_x = texture_x(x, y, walls);
            auto column = walls.get_scaled_column(tex_id, tex_x, column_h);
            size_t pix_x = i + W / 2;
            size_t pix_y =  H / 2 - column_h / 2;
            fb.drawColumn(column, pix_x, pix_y);
            break;
        }
    }
    draw_map(fb, walls, map, cell_w, cell_h);
}

const float Pi = std::atan(1.0f)*4;

int main()
{
    sf::RenderWindow window{ sf::VideoMode{W, H}, "tiny raycaster" }; 
    sf::Texture texture{};
    texture.create(W, H);

    sf::Image walls;
    walls.loadFromFile("walltext.png");
    auto[walls_x, walls_y] = walls.getSize();

    GameState gs{ 
        Map(), 
        Player{3.456f, 2.345f, 1.523f, Pi/3.f, 0, 0}, 
        Texture{ walls, (short)walls_y, (short)(walls_x / walls_y) }
    };

    FrameBuffer fb;
    fb.clear(color(0, 255, 0));
    for (auto x = 0u; x < H; ++x)
    {
        fb.at(x, x) = color(255, 0, 0);
    }

    sf::Clock clock;
    sf::Font defaultFont;
    defaultFont.loadFromFile(R"(c:\windows\fonts\arial.ttf)");
    sf::Text txt;
    txt.setFont(defaultFont);
    txt.setFillColor(sf::Color::Black);
    txt.setStyle(sf::Text::Bold);
    txt.setCharacterSize(42);
    txt.setString(L"Test");
    while (window.isOpen())
    {
        sf::Event e;
        while (window.pollEvent(e))
        {
            if (e.type == sf::Event::Closed) 
            {
                window.close();
            }
            if (e.type == sf::Event::KeyPressed || e.type == sf::Event::KeyReleased)
            {
                switch(e.key.code)
                {
                case sf::Keyboard::Escape:
                        window.close();
                        break;
                case sf::Keyboard::A:
                case sf::Keyboard::Left:
                    gs.player.a -= 0.1f;
                    break;
                case sf::Keyboard::D:
                case sf::Keyboard::Right:
                    gs.player.a += 0.1f;
                    break;
                }
            }
        }
        {
            std::wstringstream s;
            clock.restart();
            render(gs, fb);
            s << clock.restart().asMilliseconds()<<L"ms";
            txt.setString(s.str());
        }
        fb.drawTo(texture);
        sf::Sprite sprite{ texture };
        window.draw(sprite);
        window.draw(txt);
        window.display();
    }
	return 0;
}
