#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "gb.h"

#define SCALE  5

// RRGGBBAA
#define HEX_TO_COLOR(x) (x >> 24)&0xff, (x >> 16)&0xff, (x >> 8)&0xff, (x)&0xff
#define BLACK   0x000000FF
#define WHITE   0xFFFFFFFF
#define RED     0xFF0000FF
#define GREEN   0x00FF00FF
#define BLUE    0x0000FFFF
#define YELLOW  0xFFFF00FF
#define MAGENTA 0xFF00FFFF
#define CYAN    0x00FFFFFF

#define BG          0x131313FF
#define DBG_GRID    0x222222FF
#define DBG_VIEW    0xFFFF14FF

typedef enum ViewerType {
    VT_GAME,
    VT_TILEMAP,
    VT_TILES,
} ViewerType;

static SDL_Window *window;
static ViewerType viewer_type = VT_GAME;

static void render_debug_tile(SDL_Renderer *renderer, uint8_t *tile, int x, int y, int w, int h)
{
    // 8x8 pixels
    static const uint8_t PALETTE[] = {0xFF, 0x80, 0x40, 0x00};
    for (int row = 0; row < 8; row++) {
        uint8_t low_bitplane  = tile[row*2+0];
        uint8_t high_bitplane = tile[row*2+1];
        for (int col = 0; col < 8; col++) {
            uint8_t bit0 = (low_bitplane & 0x80) >> 7;
            uint8_t bit1 = (high_bitplane & 0x80) >> 7;
            low_bitplane <<= 1;
            high_bitplane <<= 1;

            uint8_t color_idx = (bit1 << 1) | bit0;
            uint8_t color = PALETTE[color_idx];
            SDL_SetRenderDrawColor(renderer, color, color, color, 0xFF);
            int xpixel = x+col*(w/8);
            int ypixel = y+row*(h/8);
            SDL_Rect r = { xpixel, ypixel, w/8, h/8};
            SDL_RenderFillRect(renderer, &r);
        }
    }
}

static void render_debug_tiles_section(SDL_Renderer *renderer, uint8_t *tiles, int x, int y, int w, int h)
{
    // 16x8 tiles
    SDL_Rect r = { x, y, w, h};
    SDL_RenderFillRect(renderer, &r);

    float w_tile = w/16.0f;
    float h_tile = h/8.0f;
    int tile_dim = (w_tile > h_tile) ? h_tile : w_tile;

    int xstart = (w - tile_dim*16)/2;
    int ystart = (h - tile_dim*8)/2;
    x += xstart;
    y += ystart;

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 16; col++) {
            SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(MAGENTA));
            int xtile = x+col*tile_dim;
            int ytile = y+row*tile_dim;
            SDL_Rect r = { xtile, ytile, tile_dim, tile_dim};
            SDL_RenderFillRect(renderer, &r);

            int tile_idx = row*16 + col; // 0-127
            uint8_t *tile = tiles + tile_idx*16;
            render_debug_tile(renderer, tile, xtile, ytile, tile_dim, tile_dim);
        }
    }
}

static void render_debug_tiles(GameBoy *gb, SDL_Renderer *renderer, int w, int h)
{
    // 3 sections of 128 tiles (16x8 tiles)
    if (viewer_type == VT_TILES) {
        // Render tiles at $8000-$87FF
        {
        SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(BG));
        uint8_t *tiles = gb->memory + 0x8000;
        render_debug_tiles_section(renderer, tiles, 0, (h/3)*0, w, h/3);
        }

        // Render tiles at $8800-$8FFF
        {
        SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(BG));
        uint8_t *tiles = gb->memory + 0x8800;
        render_debug_tiles_section(renderer, tiles, 0, (h/3)*1, w, h/3);
        }

        // Render tiles at $9000-$97FF
        {
        SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(BG));
        uint8_t *tiles = gb->memory + 0x9000;
        render_debug_tiles_section(renderer, tiles, 0, (h/3)*2, w, h/3);
        }
    }
}

static void render_debug_tile_grid(SDL_Renderer *renderer, int pixel_dim, int x, int y)
{
    if (true /*show_tile_grid*/) {
        SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(DBG_GRID));
        for (int i = 0; i <= 32; i++) {
            int yline = y+i*TILE_PIXELS*pixel_dim;
            SDL_RenderDrawLine(renderer,
                x, yline,
                x+32*TILE_PIXELS*pixel_dim, yline);
        }
        for (int i = 0; i <= 32; i++) {
            int xline = x+i*TILE_PIXELS*pixel_dim;
            SDL_RenderDrawLine(renderer,
                xline, y,
                xline, y+32*TILE_PIXELS*pixel_dim);
        }
    }
}

static void render_debug_viewport(SDL_Renderer *renderer, int pixel_dim, int x, int y)
{
    SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(DBG_VIEW));
    int thick = 5;
    SDL_Rect r = {x, y+HEIGHT*pixel_dim, WIDTH*pixel_dim+thick, thick};
    SDL_RenderFillRect(renderer, &r);
    r = (SDL_Rect){x+WIDTH*pixel_dim, y, thick, HEIGHT*pixel_dim};
    SDL_RenderFillRect(renderer, &r);
}

void render_debug_tilemap(GameBoy *gb, SDL_Renderer *renderer, int w, int h)
{
    if (viewer_type == VT_TILEMAP) {
        int min_dim = h < w ? h : w;
        int pixel_dim = min_dim / 256; // GameBoy pixel size
        int x = (w - (256*pixel_dim))/2;
        int y = (h - (256*pixel_dim))/2;
        for (int row = 0; row < 256; row++) {
            for (int col = 0; col < 256; col++) {
                uint8_t color = gb->display[row*256 + col];
                SDL_SetRenderDrawColor(renderer, color, color, color, 255);
                SDL_Rect r = {
                    x+col*pixel_dim, y+row*pixel_dim,
                    pixel_dim, pixel_dim};
                SDL_RenderFillRect(renderer, &r);
            }
        }

        render_debug_viewport(renderer, pixel_dim, x, y);
        render_debug_tile_grid(renderer, pixel_dim, x, y);
    }
}

void sdl_render(GameBoy *gb, SDL_Renderer *renderer)
{
    if (gb->memory[rLCDC] == 0) return;

    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(BG));
    SDL_RenderClear(renderer);

    int min_dim = h < w ? h : w;
    int pixel_dim = min_dim / 160; // GameBoy pixel size
    int x = (w - (160*pixel_dim))/2;
    int y = (h - (144*pixel_dim))/2;
    if (viewer_type == VT_GAME) {
        uint16_t scx = gb->memory[rSCX];
        uint16_t scy = gb->memory[rSCY];
        for (int row = 0; row < 144; row++) {
            uint16_t px_row = (row+scy) % 256;
            for (int col = 0; col < 160; col++) {
                uint16_t px_col = (col+scx) % 256;
                uint8_t color = gb->display[(px_row)*256 + (px_col)];
                SDL_SetRenderDrawColor(renderer, color, color, color, 255);
                SDL_Rect r = {
                    x+col*pixel_dim, y+row*pixel_dim,
                    pixel_dim, pixel_dim};
                SDL_RenderFillRect(renderer, &r);
            }
        }
    }

    // Debug rendering
    render_debug_tilemap(gb, renderer, w, h);
    render_debug_tiles(gb, renderer, w, h);

    SDL_RenderPresent(renderer);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <path to ROM>\n", argv[0]);
        exit(1);
    }

    GameBoy gb = {0};
    gb.running = true;
    gb.printf = printf;
    gb_load_rom_file(&gb, argv[1]);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Failed to initialize SDL\n");
        exit(1);
    }

    int width = SCALE*WIDTH;
    int height = SCALE*HEIGHT;
    window = SDL_CreateWindow("GameBoy Emulator", 0, 0, width, height, 0/*SDL_WINDOW_RESIZABLE*/);
    if (!window) {
        fprintf(stderr, "Failed to create window\n");
        exit(1);
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        fprintf(stderr, "Failed to create renderer\n");
        exit(1);
    }

    SDL_Event e;
    Uint64 counter_freq = SDL_GetPerformanceFrequency();
    Uint64 start_counter = SDL_GetPerformanceCounter();
    Uint64 prev_counter = start_counter;
    double frame_ms = 0;
    while (gb.running) {
        Uint64 curr_counter = SDL_GetPerformanceCounter();
        Uint64 delta_counter = curr_counter - prev_counter;
        prev_counter = curr_counter;
        double dt_ms = (double)delta_counter / (counter_freq / 1000.0);
        frame_ms += dt_ms;
        //printf("dt: %f ms\n", dt_ms);

        // Input
#if 1
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                gb.running = false;
            } else if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    if (e.window.data1 < 256 || e.window.data2 < 256) {
                        //SDL_SetWindowSize(window, 256, 256);
                    }
                }
            } else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                switch (e.key.keysym.sym) {
                case SDLK_ESCAPE:
                    gb.running = false;
                    break;
                case SDLK_g:
                    viewer_type = VT_GAME;
                    break;
                case SDLK_m:
                    viewer_type = VT_TILEMAP;
                    break;
                case SDLK_t:
                    viewer_type = VT_TILES;
                    break;
                case SDLK_p:
                    if (e.key.type == SDL_KEYDOWN) {
                        gb.paused = !gb.paused;
                        if (gb.paused) gb.step_debug = true;
                    }
                    break;
                case SDLK_SPACE:
                    if (e.key.type == SDL_KEYDOWN) gb_dump(&gb);
                    break;
                case SDLK_s:
                    gb.button_a = e.key.type == SDL_KEYDOWN ? 1 : 0;
                    break;
                case SDLK_a:
                    gb.button_b = e.key.type == SDL_KEYDOWN ? 1 : 0;
                    break;
                case SDLK_UP:
                    gb.dpad_up = e.key.type == SDL_KEYDOWN ? 1 : 0;
                    break;
                case SDLK_DOWN:
                    gb.dpad_down = e.key.type == SDL_KEYDOWN ? 1 : 0;
                    break;
                case SDLK_RIGHT:
                    gb.dpad_right = e.key.type == SDL_KEYDOWN ? 1 : 0;
                    break;
                case SDLK_LEFT:
                    gb.dpad_left = e.key.type == SDL_KEYDOWN ? 1 : 0;
                    break;
                default:
                    break;
                }
            }
        }
#endif

        // Update
        //if ((gb.inst_executed % 100000) == 0)
        //    fprintf(stderr, "Inst Count: %lu\n", gb.inst_executed);
        for (int i = 0; i < 1; i++) {
            gb_tick(&gb, dt_ms);
        }
        //continue;

        // Render
        if (frame_ms > 16.0) {
            sdl_render(&gb, renderer);
            frame_ms -= 16.0;
        }
    }

    return 0;
}
