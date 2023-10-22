#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <SDL.h>

// UI Elements:
// [ ] - Button
// [ ] - Panel

#define BIT_CLR(x, index) (x) &= ~(1 << (index))
#define BIT_SET(x, index) (x) |=  (1 << (index))
#define BIT_ASSIGN(x, index, value) do {                  \
    if (value) BIT_SET(x, index); else BIT_CLR(x, index); \
} while (false)

#define WIDTH 800
#define HEIGHT 600
//#define DESIRED_ASPECT (16.0 / 9.0)
#define DESIRED_ASPECT (160.0 / 144.0)

// AARRGGBB
#define HEX_TO_COLOR(x) (x >> 16)&0xff, (x >> 8)&0xff, (x >> 0)&0xff, (x >> 24)&0xff
#define BLACK   0xff000000
#define WHITE   0xffffffff
#define RED     0xffff0000
#define GREEN   0xff00ff00
#define BLUE    0xff0000ff
#define YELLOW  0xffffff00
#define MAGENTA 0xffff00ff
#define CYAN    0xff00ffff

#define BG      0xff131313

#define BTN_INACTIVE 0xff1f1f1f
#define BTN_HOVER    0xff2f2f2f
#define BTN_ACTIVE   0xff3f3f3f

// Types
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int64_t  s64;

typedef float  f32;
typedef double f64;

typedef u32 Color;

typedef struct ui_rect_t {
    f32 x, y, w, h;
} ui_rect_t;

typedef struct ui_button_t {
    size_t id;
    bool active;
} ui_button_t;

typedef struct ui_ctx_t {
    bool is_key_down;
    bool is_mouse_down;
    int mouse_x, mouse_y;

    ui_rect_t win_rect;
    ui_rect_t view_rect;

    size_t button_count;
    size_t button_last_active;
    ui_button_t buttons[32];

    bool show_modal;
} ui_ctx_t;

// What about non-square tiles like 5x7 ... ???
typedef struct Tile1bpp {
    u8 data[8*1]; // 8x8 tiles, each row encoded with 1 byte
} Tile1bpp;

typedef struct Tile2bpp {
    u8 data[8*2]; // 8x8 tiles, each row encoded with 2 bytes
} Tile2bpp;

typedef struct Tile4bpp {
    u8 data[8*4]; // 8x8 tiles, each row encoded with 4 bytes
} Tile4bpp;

typedef struct Palette1bpp {
    Color colors[2];
} Palette1bpp;

typedef struct Palette2bpp {
    Color colors[4];
} Palette2bpp;

typedef struct Tilemap {
    u16 tile_count;     // number of tiles in tilemap (0-65535)
    u8 tile_w;          // width  in pixels
    u8 tile_h;          // height in pixels
    u8 tile_bpp;        // bits per pixel: 1, 2, 4, 8
    u8 tile_size;       // bytes per tile: (u8)ceil(((tile_w * tile_h) * tile_bpp) / 8.0)
    u32 data_size;      // size in bytes of data: tile_count*tile_size
    u8 *data;           // [tile 0][tile 1]...[tile n]
} Tilemap;

typedef struct Font1bpp {
    Tile1bpp tiles[96]; // ASCII printable characters 32-127 (' ' .. '~')
} Font1bpp;

typedef struct Font2bpp {
    Tile2bpp tiles[96]; // ASCII printable characters 32-127 (' ' .. '~')
} Font2bpp;

// Globals
extern const u8 font_tiles[96 * 16];

static bool running;
static ui_ctx_t ui_ctx;
static SDL_Window *window;
static SDL_Renderer *renderer;

f32 clamp(f32 in, f32 min, f32 max);
f32 remap(f32 in, f32 in_min, f32 in_max, f32 out_min, f32 out_max);

void ui_clear(Color color);
void ui_present(void);
void ui_viewport(f32 x, f32 y, f32 w, f32 h);

void ui_draw_rect(int x, int y, int w, int h, Color c);
void ui_draw_line(int x1, int y1, int x2, int y2, Color c);
void ui_draw_tile_1bpp(Tile1bpp tile, int x, int y, Palette1bpp plt, int px_size, bool outline);
void ui_draw_tile_2bpp(Tile2bpp tile, int x, int y, Palette2bpp plt, int px_size, bool outline);
Tile2bpp ui_convert_tile_1bpp_to_2bpp(Tile1bpp tile);

bool ui_point_in_rect(f32 x, f32 y, f32 w, f32 h, f32 point_x, f32 point_y);

void ui_rect(f32 x, f32 y, f32 w, f32 h, Color color);
void ui_rect_ex(ui_rect_t r, Color color);
bool ui_button(f32 x, f32 y, f32 w, f32 h);
bool ui_text_button(f32 x, f32 y, f32 w, f32 h, const char *text, bool inactive);
ui_rect_t ui_text(f32 x, f32 y, Color color, const char *text, u32 pixel_scale, bool display);

void test(void);

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    test();

    SDL_Init(SDL_INIT_VIDEO);
    ui_ctx.win_rect = (ui_rect_t){0.0, 0.0, WIDTH, HEIGHT};
    SDL_CreateWindowAndRenderer(WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer);

    running = true;
    ui_ctx.mouse_x = -1;
    ui_ctx.mouse_y = -1;
    ui_ctx.button_last_active = (size_t)-1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT: running = false; break;
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP: {
                    u8 button = e.button.button;
                    switch (button) {
                        case SDL_BUTTON_LEFT: {
                            ui_ctx.is_mouse_down = e.type == SDL_MOUSEBUTTONDOWN;
                            ui_ctx.mouse_x = e.button.x;
                            ui_ctx.mouse_y = e.button.y;
                        } break;
                        default: break;
                    }
                } break;
                case SDL_MOUSEMOTION: {
                    ui_ctx.mouse_x = e.motion.x;
                    ui_ctx.mouse_y = e.motion.y;
                } break;
                case SDL_KEYDOWN:
                case SDL_KEYUP: {
                    ui_ctx.is_key_down = e.type == SDL_KEYDOWN;
                    SDL_Keycode key = e.key.keysym.sym;
                    switch (key) {
                        case SDLK_ESCAPE: {
                            if (ui_ctx.is_key_down) ui_ctx.show_modal = !ui_ctx.show_modal;
                        } break;
                        case SDLK_q: running = false; break;
                        case SDLK_SPACE: {
                            if (ui_ctx.is_key_down) printf("%d x %d\n", (u32)ui_ctx.win_rect.w, (u32)ui_ctx.win_rect.h);
                        } break;
                        default: break;
                    }
                } break;
                case SDL_WINDOWEVENT: {
                    switch (e.window.event) {
                        case SDL_WINDOWEVENT_SIZE_CHANGED: {
                            ui_ctx.win_rect.w = e.window.data1;
                            ui_ctx.win_rect.h = e.window.data2;
                        } break;
                        default: break;
                    }
                } break;
                default: break;
            }
        }

        // Render
        ui_clear(BG);
        
        ui_viewport(0.0, 0.0, ui_ctx.win_rect.w, ui_ctx.win_rect.h);

        f32 view_x, view_y, view_w, view_h;
        view_x = view_y = view_w = view_h = 0.0;

        f32 aspect = (f32)ui_ctx.win_rect.w / (f32)ui_ctx.win_rect.h;
        if (aspect < DESIRED_ASPECT) {
            // What should be the height to have an aspect of 16:9 ?
            //  800 x  600      => 1.3333
            // 1920 x 1080      => 1.7777
            //    w x h         => DESIRED_ASPECT
            //    w x ?         => DESIRED_ASPECT   => h = w / DESIRED_ASPECT 
            //    ? x h         => DESIRED_ASPECT   => w = h * DESIRED_ASPECT 
            f32 h = ui_ctx.win_rect.w / DESIRED_ASPECT;        // Required height to keep a 16:9 aspect
            view_h = h;
            view_w = view_h * DESIRED_ASPECT;
            h = remap(h, 0.0, ui_ctx.win_rect.h, 0.0, 1.0);   // Required height normalized
            h = (1.0 - h) / 2.0;                            // Border height
            ui_rect(0.0, 0.0, 1.0, h, 0xff222222);
            ui_rect(0.0, 1.0 - h, 1.0, h, 0xff222222);
        } else if (aspect > DESIRED_ASPECT) {
            // What should be the width to have an aspect of 16:9 ?
            f32 w = ui_ctx.win_rect.h * DESIRED_ASPECT;       // Required width to keep a 16:9 aspect
            view_w = w;
            view_h = view_w / DESIRED_ASPECT;
            w = remap(w, 0.0, ui_ctx.win_rect.w, 0.0, 1.0);    // Required width normalized
            w = (1.0 - w) / 2.0;                            // Border width
            ui_rect(0.0, 0.0, w, 1.0, 0xff222222);
            ui_rect(1.0 - w, 0.0, w, 1.0, 0xff222222);
        }
        view_x = (ui_ctx.win_rect.w - view_w) / 2.0;
        view_y = (ui_ctx.win_rect.h - view_h) / 2.0;
        ui_viewport(view_x, view_y, view_w, view_h);

        const char *buttons[] = {"Game", "Tiles", "Regs"};
        size_t btn_count = sizeof(buttons)/sizeof(buttons[0]);
        for (size_t i = 0; i < btn_count; i++) {
            f32 btn_w = 1.0 / btn_count;
            if (ui_text_button(i*btn_w, 0.0, btn_w, 0.05, buttons[i], ui_ctx.show_modal)) {
                printf("%s clicked\n", buttons[i]);
            }
        }
        if (ui_ctx.show_modal) {
            const char *menu_opts[] = {"Options", "Restart", "Quit"};
            size_t opts_count = sizeof(menu_opts)/sizeof(menu_opts[0]);
            for (size_t i = 0; i < opts_count; i++) {
                f32 btn_w = 0.3;
                f32 btn_h = 0.05;
                if (ui_text_button(0.35, 0.2 + i*btn_h, btn_w, btn_h, menu_opts[i], false)) {
                    printf("%s clicked\n", menu_opts[i]);
                    if (i == 2) running = false;
                }
            }
        }

        ui_draw_rect(0, 0, 8, 8, CYAN);
        ui_present();
    }

    return 0;
}

void ui_clear(Color color)
{
    SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(color));
    SDL_RenderClear(renderer);
}

void ui_present(void)
{
    SDL_RenderPresent(renderer);
    ui_ctx.button_count = 0;
}

bool ui_point_in_rect(f32 x, f32 y, f32 w, f32 h, f32 point_x, f32 point_y)
{
    ui_rect_t view_rect = ui_ctx.view_rect;
    x = remap(x, 0.0, 1.0, view_rect.x, view_rect.x + view_rect.w);
    y = remap(y, 0.0, 1.0, view_rect.y, view_rect.y + view_rect.h);
    w = remap(w, 0.0, 1.0, 0.0, view_rect.w);
    h = remap(h, 0.0, 1.0, 0.0, view_rect.h);
    return (point_x >= x) && (point_x <= (x + w)) && (point_y >= y) && (point_y <= (y + h));
}

void ui_viewport(f32 x, f32 y, f32 w, f32 h)
{
    ui_ctx.view_rect.x = clamp(x, 0.0, ui_ctx.win_rect.w);
    ui_ctx.view_rect.y = clamp(y, 0.0, ui_ctx.win_rect.h);
    ui_ctx.view_rect.w = clamp(w, 0.0, ui_ctx.win_rect.w);
    ui_ctx.view_rect.h = clamp(h, 0.0, ui_ctx.win_rect.h);
}

void ui_draw_rect(int x, int y, int w, int h, Color c)
{
    SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(c));
    SDL_Rect r = { x, y, w, h};
    SDL_RenderFillRect(renderer, &r);
}

void ui_draw_line(int x1, int y1, int x2, int y2, Color c)
{
    SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(c));
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

void ui_rect(f32 x, f32 y, f32 w, f32 h, Color color)
{
    // Map viewport coords to window coords
    ui_rect_t view_rect = ui_ctx.view_rect;
    x = remap(x, 0.0, 1.0, view_rect.x, view_rect.x + view_rect.w);
    y = remap(y, 0.0, 1.0, view_rect.y, view_rect.y + view_rect.h);
    w = remap(w, 0.0, 1.0, 0.0, view_rect.w);
    h = remap(h, 0.0, 1.0, 0.0, view_rect.h);

    SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(color));
    SDL_Rect r = { x, y, w, h};
    SDL_RenderFillRect(renderer, &r);
}

void ui_rect_ex(ui_rect_t r, Color color)
{
    ui_rect(r.x, r.y, r.w, r.h, color);
}

bool ui_button(f32 x, f32 y, f32 w, f32 h)
{
    ui_button_t *button = &ui_ctx.buttons[ui_ctx.button_count++];

    bool hover = ui_point_in_rect(x, y, w, h, ui_ctx.mouse_x, ui_ctx.mouse_y);
    bool active = ui_ctx.is_mouse_down && hover;

    Color color = BTN_INACTIVE;
    if (active) color = BTN_ACTIVE;
    else if (hover) color = BTN_HOVER;

    if (ui_ctx.button_last_active == ui_ctx.button_count - 1) color = RED;

    ui_rect(x, y, w, h, color);

    if (active && !button->active) {
        button->active = active;
        ui_ctx.button_last_active = ui_ctx.button_count - 1;
        return true;
    } else {
        button->active = active;
        return false;
    }
}

bool ui_text_button(f32 x, f32 y, f32 w, f32 h, const char *text, bool inactive)
{
    ui_button_t *button = &ui_ctx.buttons[ui_ctx.button_count++];

    bool hover = ui_point_in_rect(x, y, w, h, ui_ctx.mouse_x, ui_ctx.mouse_y);
    bool active = ui_ctx.is_mouse_down && hover;

    if (inactive) {
        hover = false;
        active = false;
    }

    Color color = BTN_INACTIVE;
    if (active) color = BTN_ACTIVE;
    else if (hover) color = BTN_HOVER;

    if (ui_ctx.button_last_active == ui_ctx.button_count - 1) color = RED;

    u32 pixel_scale = 3;
    ui_rect_t extent = ui_text(x, y, WHITE, text, pixel_scale, false);
    //printf("%s %f,%f,%f,%f\n", text, extent.x, extent.y, extent.w, extent.h);
    f32 x_extra = (w - extent.w)/2.0;
    f32 y_extra = (h - extent.h)/2.0;
    ui_rect(x, y, w, h, color);
    ui_text(x + x_extra, y + y_extra, WHITE, text, pixel_scale, true);

    if (active && !button->active) {
        button->active = active;
        ui_ctx.button_last_active = ui_ctx.button_count - 1;
        return true;
    } else {
        button->active = active;
        return false;
    }
}

ui_rect_t ui_text(f32 x, f32 y, Color color, const char *text, u32 pixel_scale, bool display)
{
    f32 min_x = x;
    f32 min_y = y;
    f32 max_x = x;
    f32 max_y = y;

    ui_rect_t view_rect = ui_ctx.view_rect;
    x = remap(x, 0.0, 1.0, view_rect.x, view_rect.x + view_rect.w);
    y = remap(y, 0.0, 1.0, view_rect.y, view_rect.y + view_rect.h);

    size_t len = strlen(text);

    int px_x = x + 8*(len-1)*pixel_scale + (8-1)*pixel_scale;
    int px_y = y + (8-1)*pixel_scale;
    f32 norm_x = remap(px_x + pixel_scale, view_rect.x, view_rect.x + view_rect.w, 0.0, 1.0);
    f32 norm_y = remap(px_y + pixel_scale, view_rect.y, view_rect.y + view_rect.h, 0.0, 1.0);
    if (norm_x > max_x) max_x = norm_x;
    if (norm_y > max_y) max_y = norm_y;
    if (!display) return (ui_rect_t){min_x, min_y, max_x - min_x, max_y - min_y};

    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        if (c < ' ' || c > '~') c = '.';
        int index = c - ' ';
        const u8 *tile = font_tiles + index*16;

        for (int row = 0; row < 8; row++) {
            u8 lo_bitplane = tile[row*2 + 0];
            u8 hi_bitplane = tile[row*2 + 1];
            for (int col = 0; col < 8; col++) {
                u8 bit0 = (lo_bitplane & 0x80) >> 7;
                u8 bit1 = (hi_bitplane & 0x80) >> 7;
                lo_bitplane <<= 1;
                hi_bitplane <<= 1;

                int px_x = x + 8*i*pixel_scale + col*pixel_scale;
                int px_y = y + row*pixel_scale;

                u8 color_idx = (bit1 << 1) | bit0;
                if (display && color_idx != 0) {
                    SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(color));
                    SDL_Rect r = { px_x, px_y, pixel_scale, pixel_scale };
                    SDL_RenderFillRect(renderer, &r);
                }
            }
        }
    }

    return (ui_rect_t){min_x, min_y, max_x - min_x, max_y - min_y};
}

f32 clamp(f32 in, f32 min, f32 max)
{
    if (in < min) return min;
    if (in > max) return max;
    return in;
}

f32 remap(f32 in, f32 in_min, f32 in_max, f32 out_min, f32 out_max)
{
    in = clamp(in, in_min, in_max);
    f32 normalized = (in - in_min) / (in_max - in_min);
    return out_min + normalized * (out_max - out_min);
}

void test(void)
{
    // Clamp
    assert(clamp(0.0, 0.0, 1.0) == 0.0);
    assert(clamp(0.5, 0.0, 1.0) == 0.5);
    assert(clamp(1.0, 0.0, 1.0) == 1.0);
    assert(clamp(0.0, 1.0, 2.0) == 1.0);
    assert(clamp(2.0, 0.0, 1.0) == 1.0);

    // Remap
    assert(remap(  0.0f,   0.0f, 800.0f,   0.0f,   1.0f) ==   0.0f);
    assert(remap(400.0f,   0.0f, 800.0f,   0.0f,   1.0f) ==   0.5f);
    assert(remap(800.0f,   0.0f, 800.0f,   0.0f,   1.0f) ==   1.0f);
    assert(remap(  0.0f,   0.0f,   1.0f,   0.0f, 800.0f) ==   0.0f);
    assert(remap(  0.5f,   0.0f,   1.0f,   0.0f, 800.0f) == 400.0f);
    assert(remap(  1.0f,   0.0f,   1.0f,   0.0f, 800.0f) == 800.0f);
}


void ui_draw_tile_1bpp(Tile1bpp tile, int x, int y, Palette1bpp plt, int px_size, bool outline)
{
    for (int row = 0; row < 8; row++) {
        u8 tile_data = tile.data[row];
        for (int col = 0; col < 8; col++) {
            u8 col_idx = (tile_data & 0x80) >> 7;
            tile_data <<= 1;
            Color c = plt.colors[col_idx];
            int x0 = x + col*px_size;
            int y0 = y + row*px_size;
            ui_draw_rect(x0, y0, px_size, px_size, c);

            if (outline) {
                int x1 = x0 + px_size - 1;
                int y1 = y0 + px_size - 1;
                ui_draw_line(x0, y0, x1, y0, BLACK);
                ui_draw_line(x0, y1, x1, y1, BLACK);
                ui_draw_line(x0, y0, x0, y1, BLACK);
                ui_draw_line(x1, y0, x1, y1, BLACK);
            }
        }
    }
}

void ui_draw_tile_2bpp(Tile2bpp tile, int x, int y, Palette2bpp plt, int px_size, bool outline)
{
    for (int row = 0; row < 8; row++) {
        u8 tile_data_0 = tile.data[row*2 + 0];
        u8 tile_data_1 = tile.data[row*2 + 1];
        for (int col = 0; col < 8; col++) {
            u8 col_idx = ((tile_data_0 & 0x80) >> 7) | ((tile_data_1 & 0x80) >> 6);
            tile_data_0 <<= 1;
            tile_data_1 <<= 1;
            Color c = plt.colors[col_idx];
            int x0 = x + col*px_size;
            int y0 = y + row*px_size;
            ui_draw_rect(x0, y0, px_size, px_size, c);

            if (outline) {
                int x1 = x0 + px_size - 1;
                int y1 = y0 + px_size - 1;
                ui_draw_line(x0, y0, x1, y0, BLACK);
                ui_draw_line(x0, y1, x1, y1, BLACK);
                ui_draw_line(x0, y0, x0, y1, BLACK);
                ui_draw_line(x1, y0, x1, y1, BLACK);
            }
        }
    }
}

Tile2bpp ui_convert_tile_1bpp_to_2bpp(Tile1bpp tile)
{
    Tile2bpp out = {0};
    for (int i = 0; i < 8; i++) {
        out.data[i*2 + 0] = tile.data[i];
        out.data[i*2 + 1] = tile.data[i];
    }
    return out;
}

// dw `01230123 ; This is equivalent to `db $55,$33`
// 55 => 0101 0101 low  bit-plane
// 33 => 0011 0011 high bit-plane
// ---------------
//       0123 0123

const u8 font_tiles[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ' '
    0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, // '!'
    0x6C, 0x6C, 0x6C, 0x6C, 0x6C, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // '"'
    0x6C, 0x6C, 0xFE, 0xFE, 0x6C, 0x6C, 0x6C, 0x6C, 0xFE, 0xFE, 0x6C, 0x6C, 0x00, 0x00, 0x00, 0x00, // '#'
    0x18, 0x18, 0x3E, 0x3E, 0x60, 0x60, 0x3C, 0x3C, 0x06, 0x06, 0x7C, 0x7C, 0x18, 0x18, 0x00, 0x00, // '$'
    0x66, 0x66, 0x6C, 0x6C, 0x18, 0x18, 0x30, 0x30, 0x60, 0x60, 0xC6, 0xC6, 0x86, 0x86, 0x00, 0x00, // '%'
    0x1C, 0x1C, 0x36, 0x36, 0x1C, 0x1C, 0x38, 0x38, 0x6F, 0x6F, 0x66, 0x66, 0x3B, 0x3B, 0x00, 0x00, // '&'
    0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // '''
    0x0E, 0x0E, 0x1C, 0x1C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1C, 0x1C, 0x0E, 0x0E, 0x00, 0x00, // '('
    0x70, 0x70, 0x38, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x38, 0x38, 0x70, 0x70, 0x00, 0x00, // ')'
    0x00, 0x00, 0x66, 0x66, 0x3C, 0x3C, 0xFF, 0xFF, 0x3C, 0x3C, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00, // '*'
    0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, // '+'
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x30, 0x30, 0x60, 0x60, // ','
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // '-'
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0x60, 0x60, 0x00, 0x00, // '.'
    0x02, 0x02, 0x06, 0x06, 0x0C, 0x0C, 0x18, 0x18, 0x30, 0x30, 0x60, 0x60, 0x40, 0x40, 0x00, 0x00, // '/'
    0x3C, 0x3C, 0x66, 0x66, 0x6E, 0x6E, 0x76, 0x76, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x00, 0x00, // '0'
    0x18, 0x18, 0x38, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x7E, 0x00, 0x00, // '1'
    0x3C, 0x3C, 0x66, 0x66, 0x06, 0x06, 0x0C, 0x0C, 0x18, 0x18, 0x30, 0x30, 0x7E, 0x7E, 0x00, 0x00, // '2'
    0x7E, 0x7E, 0x0C, 0x0C, 0x18, 0x18, 0x0C, 0x0C, 0x06, 0x06, 0x66, 0x66, 0x3C, 0x3C, 0x00, 0x00, // '3'
    0x0C, 0x0C, 0x1C, 0x1C, 0x3C, 0x3C, 0x6C, 0x6C, 0x7E, 0x7E, 0x0C, 0x0C, 0x0C, 0x0C, 0x00, 0x00, // '4'
    0x7E, 0x7E, 0x60, 0x60, 0x7C, 0x7C, 0x06, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3C, 0x3C, 0x00, 0x00, // '5'
    0x3C, 0x3C, 0x60, 0x60, 0x60, 0x60, 0x7C, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x00, 0x00, // '6'
    0x7E, 0x7E, 0x06, 0x06, 0x0C, 0x0C, 0x18, 0x18, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, // '7'
    0x3C, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x00, 0x00, // '8'
    0x3C, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x3E, 0x06, 0x06, 0x0C, 0x0C, 0x38, 0x38, 0x00, 0x00, // '9'
    0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, // ':'
    0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x30, 0x30, 0x00, 0x00, // ';'
    0x0C, 0x0C, 0x18, 0x18, 0x30, 0x30, 0x60, 0x60, 0x30, 0x30, 0x18, 0x18, 0x0C, 0x0C, 0x00, 0x00, // '<'
    0x00, 0x00, 0x00, 0x00, 0x7E, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x7E, 0x00, 0x00, 0x00, 0x00, // '='
    0x30, 0x30, 0x18, 0x18, 0x0C, 0x0C, 0x06, 0x06, 0x0C, 0x0C, 0x18, 0x18, 0x30, 0x30, 0x00, 0x00, // '>'
    0x3C, 0x3C, 0x66, 0x66, 0x06, 0x06, 0x0C, 0x0C, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, // '?'
    0x3C, 0x3C, 0x66, 0x66, 0x6E, 0x6E, 0x6A, 0x6A, 0x6E, 0x6E, 0x60, 0x60, 0x3E, 0x3E, 0x00, 0x00, // '@'
    0x18, 0x18, 0x3C, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x7E, 0x7E, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, // 'A'
    0x7C, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x7C, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x7C, 0x7C, 0x00, 0x00, // 'B'
    0x3C, 0x3C, 0x66, 0x66, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x66, 0x66, 0x3C, 0x3C, 0x00, 0x00, // 'C'
    0x78, 0x78, 0x6C, 0x6C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x6C, 0x6C, 0x78, 0x78, 0x00, 0x00, // 'D'
    0x7E, 0x7E, 0x60, 0x60, 0x60, 0x60, 0x7C, 0x7C, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x7E, 0x00, 0x00, // 'E'
    0x7E, 0x7E, 0x60, 0x60, 0x60, 0x60, 0x7C, 0x7C, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x00, 0x00, // 'F'
    0x3E, 0x3E, 0x60, 0x60, 0x60, 0x60, 0x6E, 0x6E, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x3E, 0x00, 0x00, // 'G'
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7E, 0x7E, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, // 'H'
    0x3C, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x3C, 0x00, 0x00, // 'I'
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3C, 0x3C, 0x00, 0x00, // 'J'
    0x66, 0x66, 0x6C, 0x6C, 0x78, 0x78, 0x70, 0x70, 0x78, 0x78, 0x6C, 0x6C, 0x66, 0x66, 0x00, 0x00, // 'K'
    0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x7E, 0x00, 0x00, // 'L'
    0xC6, 0xC6, 0xEE, 0xEE, 0xFE, 0xFE, 0xD6, 0xD6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00, // 'M'
    0x66, 0x66, 0x76, 0x76, 0x7E, 0x7E, 0x7E, 0x7E, 0x6E, 0x6E, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, // 'N'
    0x3C, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x00, 0x00, // 'O'
    0x7C, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x7C, 0x7C, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x00, 0x00, // 'P'
    0x3C, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x76, 0x76, 0x6C, 0x6C, 0x36, 0x36, 0x00, 0x00, // 'Q'
    0x7C, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x7C, 0x7C, 0x6C, 0x6C, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, // 'R'
    0x3C, 0x3C, 0x66, 0x66, 0x60, 0x60, 0x3C, 0x3C, 0x06, 0x06, 0x66, 0x66, 0x3C, 0x3C, 0x00, 0x00, // 'S'
    0x7E, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, // 'T'
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x3E, 0x00, 0x00, // 'U'
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x00, // 'V'
    0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xD6, 0xD6, 0xFE, 0xFE, 0xEE, 0xEE, 0xC6, 0xC6, 0x00, 0x00, // 'W'
    0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x18, 0x18, 0x3C, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, // 'X'
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, // 'Y'
    0x7E, 0x7E, 0x06, 0x06, 0x0C, 0x0C, 0x18, 0x18, 0x30, 0x30, 0x60, 0x60, 0x7E, 0x7E, 0x00, 0x00, // 'Z'
    0x1E, 0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x1E, 0x00, 0x00, // '['
    0x40, 0x40, 0x60, 0x60, 0x30, 0x30, 0x18, 0x18, 0x0C, 0x0C, 0x06, 0x06, 0x02, 0x02, 0x00, 0x00, // '\'
    0x78, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x78, 0x78, 0x00, 0x00, // ']'
    0x10, 0x10, 0x38, 0x38, 0x6C, 0x6C, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // '^'
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xFE, // '_'
    0xC0, 0xC0, 0x60, 0x60, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // '`'
    0x00, 0x00, 0x00, 0x00, 0x3C, 0x3C, 0x06, 0x06, 0x3E, 0x3E, 0x66, 0x66, 0x3E, 0x3E, 0x00, 0x00, // 'a'
    0x60, 0x60, 0x60, 0x60, 0x7C, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7C, 0x7C, 0x00, 0x00, // 'b'
    0x00, 0x00, 0x00, 0x00, 0x3C, 0x3C, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x3C, 0x3C, 0x00, 0x00, // 'c'
    0x06, 0x06, 0x06, 0x06, 0x3E, 0x3E, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x3E, 0x00, 0x00, // 'd'
    0x00, 0x00, 0x00, 0x00, 0x3C, 0x3C, 0x66, 0x66, 0x7E, 0x7E, 0x60, 0x60, 0x3C, 0x3C, 0x00, 0x00, // 'e'
    0x1C, 0x1C, 0x30, 0x30, 0x7C, 0x7C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, // 'f'
    0x00, 0x00, 0x00, 0x00, 0x3E, 0x3E, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x3E, 0x06, 0x06, 0x7C, 0x7C, // 'g'
    0x60, 0x60, 0x60, 0x60, 0x7C, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, // 'h'
    0x18, 0x18, 0x00, 0x00, 0x38, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x3C, 0x00, 0x00, // 'i'
    0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x70, 0x70, // 'j'
    0x60, 0x60, 0x60, 0x60, 0x66, 0x66, 0x6C, 0x6C, 0x78, 0x78, 0x6C, 0x6C, 0x66, 0x66, 0x00, 0x00, // 'k'
    0x38, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x3C, 0x00, 0x00, // 'l'
    0x00, 0x00, 0x00, 0x00, 0xEC, 0xEC, 0xFE, 0xFE, 0xD6, 0xD6, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00, // 'm'
    0x00, 0x00, 0x00, 0x00, 0x7C, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, // 'n'
    0x00, 0x00, 0x00, 0x00, 0x3C, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x00, 0x00, // 'o'
    0x00, 0x00, 0x00, 0x00, 0x7C, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7C, 0x7C, 0x60, 0x60, // 'p'
    0x00, 0x00, 0x00, 0x00, 0x3E, 0x3E, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x3E, 0x06, 0x06, // 'q'
    0x00, 0x00, 0x00, 0x00, 0x7C, 0x7C, 0x66, 0x66, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x00, 0x00, // 'r'
    0x00, 0x00, 0x00, 0x00, 0x3E, 0x3E, 0x60, 0x60, 0x3C, 0x3C, 0x06, 0x06, 0x7C, 0x7C, 0x00, 0x00, // 's'
    0x00, 0x00, 0x18, 0x18, 0x7E, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x0E, 0x0E, 0x00, 0x00, // 't'
    0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x3E, 0x00, 0x00, // 'u'
    0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x00, // 'v'
    0x00, 0x00, 0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xD6, 0xD6, 0x7C, 0x7C, 0x6C, 0x6C, 0x00, 0x00, // 'w'
    0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x3C, 0x3C, 0x18, 0x18, 0x3C, 0x3C, 0x66, 0x66, 0x00, 0x00, // 'x'
    0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x3E, 0x06, 0x06, 0x7C, 0x7C, // 'y'
    0x00, 0x00, 0x00, 0x00, 0x7E, 0x7E, 0x0C, 0x0C, 0x18, 0x18, 0x30, 0x30, 0x7E, 0x7E, 0x00, 0x00, // 'z'
    0x0E, 0x0E, 0x18, 0x18, 0x18, 0x18, 0x30, 0x30, 0x18, 0x18, 0x18, 0x18, 0x0E, 0x0E, 0x00, 0x00, // '{'
    0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, // '|'
    0x70, 0x70, 0x18, 0x18, 0x18, 0x18, 0x0C, 0x0C, 0x18, 0x18, 0x18, 0x18, 0x70, 0x70, 0x00, 0x00, // '}'
    0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0xF2, 0xF2, 0x9E, 0x9E, 0x0C, 0x0C, 0x00, 0x00, 0x00, 0x00, // '~'
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // DEL
};
