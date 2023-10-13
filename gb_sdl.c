#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "gb.h"

#define SCALE  8
#define SAMPLE_RATE 48000
#define VOLUME 0.25

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
//#define DBG_VIEW    0xFFFF14FF
#define DBG_VIEW    0xFF0014FF

typedef enum ViewerType {
    VT_GAME,
    VT_TILEMAP,
    VT_TILES,
    VT_REGS,
} ViewerType;

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_AudioDeviceID device;
static ViewerType viewer_type = VT_GAME;

static f64 frame_ms;

static void render_debug_tile(SDL_Renderer *renderer, u8 *tile, int x, int y, int w, int h)
{
    // 8x8 pixels
    for (int row = 0; row < 8; row++) {
        u8 low_bitplane  = tile[row*2+0];
        u8 high_bitplane = tile[row*2+1];
        for (int col = 0; col < 8; col++) {
            u8 bit0 = (low_bitplane & 0x80) >> 7;
            u8 bit1 = (high_bitplane & 0x80) >> 7;
            low_bitplane <<= 1;
            high_bitplane <<= 1;

            u8 color_idx = (bit1 << 1) | bit0;
            Color color = PALETTE[color_idx];
            SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(color));

            int xpixel = x+col*(w/8);
            int ypixel = y+row*(h/8);
            SDL_Rect r = { xpixel, ypixel, w/8, h/8};
            SDL_RenderFillRect(renderer, &r);
        }
    }
}

static void render_debug_tiles_section(SDL_Renderer *renderer, u8 *tiles, int x, int y, int w, int h)
{
    // 16x8 tiles
    SDL_Rect r = { x, y, w, h};
    SDL_RenderFillRect(renderer, &r);

    f32 w_tile = w/16.0f;
    f32 h_tile = h/8.0f;
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
            u8 *tile = tiles + tile_idx*16;
            render_debug_tile(renderer, tile, xtile, ytile, tile_dim, tile_dim);
        }
    }
}

static void render_debug_tiles(SDL_Renderer *renderer, int w, int h, u8 *tile_data)
{
    // 3 sections of 128 tiles (16x8 tiles)
    // Render tiles at $8000-$87FF
    {
        SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(BG));
        //u8 *tiles = gb->memory + 0x8000;
        u8 *tiles = tile_data;
        render_debug_tiles_section(renderer, tiles, 0, (h/3)*0, w, h/3);
    }

    // Render tiles at $8800-$8FFF
    {
        SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(BG));
        //u8 *tiles = gb->memory + 0x8800;
        u8 *tiles = tile_data + 0x800;
        render_debug_tiles_section(renderer, tiles, 0, (h/3)*1, w, h/3);
    }

    // Render tiles at $9000-$97FF
    {
        SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(BG));
        //u8 *tiles = gb->memory + 0x9000;
        u8 *tiles = tile_data + 0x1000;
        render_debug_tiles_section(renderer, tiles, 0, (h/3)*2, w, h/3);
    }
}

static void render_debug_text(SDL_Renderer *renderer, const char *text, int row, int col)
{
    u8 tiles[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00,
        0x6C, 0x6C, 0x6C, 0x6C, 0x6C, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x6C, 0x6C, 0xFE, 0xFE, 0x6C, 0x6C, 0x6C, 0x6C, 0xFE, 0xFE, 0x6C, 0x6C, 0x00, 0x00, 0x00, 0x00,
        0x18, 0x18, 0x3E, 0x3E, 0x60, 0x60, 0x3C, 0x3C, 0x06, 0x06, 0x7C, 0x7C, 0x18, 0x18, 0x00, 0x00,
        0x66, 0x66, 0x6C, 0x6C, 0x18, 0x18, 0x30, 0x30, 0x60, 0x60, 0xC6, 0xC6, 0x86, 0x86, 0x00, 0x00,
        0x1C, 0x1C, 0x36, 0x36, 0x1C, 0x1C, 0x38, 0x38, 0x6F, 0x6F, 0x66, 0x66, 0x3B, 0x3B, 0x00, 0x00,
        0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x0E, 0x0E, 0x1C, 0x1C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1C, 0x1C, 0x0E, 0x0E, 0x00, 0x00,
        0x70, 0x70, 0x38, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x38, 0x38, 0x70, 0x70, 0x00, 0x00,
        0x00, 0x00, 0x66, 0x66, 0x3C, 0x3C, 0xFF, 0xFF, 0x3C, 0x3C, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x30, 0x30, 0x60, 0x60,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0x60, 0x60, 0x00, 0x00,
        0x02, 0x02, 0x06, 0x06, 0x0C, 0x0C, 0x18, 0x18, 0x30, 0x30, 0x60, 0x60, 0x40, 0x40, 0x00, 0x00,
        0x3C, 0x3C, 0x66, 0x66, 0x6E, 0x6E, 0x76, 0x76, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x00, 0x00,
        0x18, 0x18, 0x38, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x7E, 0x00, 0x00,
        0x3C, 0x3C, 0x66, 0x66, 0x06, 0x06, 0x0C, 0x0C, 0x18, 0x18, 0x30, 0x30, 0x7E, 0x7E, 0x00, 0x00,
        0x7E, 0x7E, 0x0C, 0x0C, 0x18, 0x18, 0x0C, 0x0C, 0x06, 0x06, 0x66, 0x66, 0x3C, 0x3C, 0x00, 0x00,
        0x0C, 0x0C, 0x1C, 0x1C, 0x3C, 0x3C, 0x6C, 0x6C, 0x7E, 0x7E, 0x0C, 0x0C, 0x0C, 0x0C, 0x00, 0x00,
        0x7E, 0x7E, 0x60, 0x60, 0x7C, 0x7C, 0x06, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3C, 0x3C, 0x00, 0x00,
        0x3C, 0x3C, 0x60, 0x60, 0x60, 0x60, 0x7C, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x00, 0x00,
        0x7E, 0x7E, 0x06, 0x06, 0x0C, 0x0C, 0x18, 0x18, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00,
        0x3C, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x00, 0x00,
        0x3C, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x3E, 0x06, 0x06, 0x0C, 0x0C, 0x38, 0x38, 0x00, 0x00,
        0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x30, 0x30, 0x00, 0x00,
        0x0C, 0x0C, 0x18, 0x18, 0x30, 0x30, 0x60, 0x60, 0x30, 0x30, 0x18, 0x18, 0x0C, 0x0C, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x7E, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x7E, 0x00, 0x00, 0x00, 0x00,
        0x30, 0x30, 0x18, 0x18, 0x0C, 0x0C, 0x06, 0x06, 0x0C, 0x0C, 0x18, 0x18, 0x30, 0x30, 0x00, 0x00,
        0x3C, 0x3C, 0x66, 0x66, 0x06, 0x06, 0x0C, 0x0C, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00,
        0x3C, 0x3C, 0x66, 0x66, 0x6E, 0x6E, 0x6A, 0x6A, 0x6E, 0x6E, 0x60, 0x60, 0x3E, 0x3E, 0x00, 0x00,
        0x18, 0x18, 0x3C, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x7E, 0x7E, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00,
        0x7C, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x7C, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x7C, 0x7C, 0x00, 0x00,
        0x3C, 0x3C, 0x66, 0x66, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x66, 0x66, 0x3C, 0x3C, 0x00, 0x00,
        0x78, 0x78, 0x6C, 0x6C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x6C, 0x6C, 0x78, 0x78, 0x00, 0x00,
        0x7E, 0x7E, 0x60, 0x60, 0x60, 0x60, 0x7C, 0x7C, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x7E, 0x00, 0x00,
        0x7E, 0x7E, 0x60, 0x60, 0x60, 0x60, 0x7C, 0x7C, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x00, 0x00,
        0x3E, 0x3E, 0x60, 0x60, 0x60, 0x60, 0x6E, 0x6E, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x3E, 0x00, 0x00,
        0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7E, 0x7E, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00,
        0x3C, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x3C, 0x00, 0x00,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3C, 0x3C, 0x00, 0x00,
        0x66, 0x66, 0x6C, 0x6C, 0x78, 0x78, 0x70, 0x70, 0x78, 0x78, 0x6C, 0x6C, 0x66, 0x66, 0x00, 0x00,
        0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x7E, 0x00, 0x00,
        0xC6, 0xC6, 0xEE, 0xEE, 0xFE, 0xFE, 0xD6, 0xD6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00,
        0x66, 0x66, 0x76, 0x76, 0x7E, 0x7E, 0x7E, 0x7E, 0x6E, 0x6E, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00,
        0x3C, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x00, 0x00,
        0x7C, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x7C, 0x7C, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x00, 0x00,
        0x3C, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x76, 0x76, 0x6C, 0x6C, 0x36, 0x36, 0x00, 0x00,
        0x7C, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x7C, 0x7C, 0x6C, 0x6C, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00,
        0x3C, 0x3C, 0x66, 0x66, 0x60, 0x60, 0x3C, 0x3C, 0x06, 0x06, 0x66, 0x66, 0x3C, 0x3C, 0x00, 0x00,
        0x7E, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00,
        0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x3E, 0x00, 0x00,
        0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x00,
        0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xD6, 0xD6, 0xFE, 0xFE, 0xEE, 0xEE, 0xC6, 0xC6, 0x00, 0x00,
        0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x18, 0x18, 0x3C, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00,
        0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00,
        0x7E, 0x7E, 0x06, 0x06, 0x0C, 0x0C, 0x18, 0x18, 0x30, 0x30, 0x60, 0x60, 0x7E, 0x7E, 0x00, 0x00,
        0x1E, 0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x1E, 0x00, 0x00,
        0x40, 0x40, 0x60, 0x60, 0x30, 0x30, 0x18, 0x18, 0x0C, 0x0C, 0x06, 0x06, 0x02, 0x02, 0x00, 0x00,
        0x78, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x78, 0x78, 0x00, 0x00,
        0x10, 0x10, 0x38, 0x38, 0x6C, 0x6C, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xFE,
        0xC0, 0xC0, 0x60, 0x60, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x3C, 0x3C, 0x06, 0x06, 0x3E, 0x3E, 0x66, 0x66, 0x3E, 0x3E, 0x00, 0x00,
        0x60, 0x60, 0x60, 0x60, 0x7C, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7C, 0x7C, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x3C, 0x3C, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x3C, 0x3C, 0x00, 0x00,
        0x06, 0x06, 0x06, 0x06, 0x3E, 0x3E, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x3E, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x3C, 0x3C, 0x66, 0x66, 0x7E, 0x7E, 0x60, 0x60, 0x3C, 0x3C, 0x00, 0x00,
        0x1C, 0x1C, 0x30, 0x30, 0x7C, 0x7C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x3E, 0x3E, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x3E, 0x06, 0x06, 0x7C, 0x7C,
        0x60, 0x60, 0x60, 0x60, 0x7C, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00,
        0x18, 0x18, 0x00, 0x00, 0x38, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x3C, 0x00, 0x00,
        0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x70, 0x70,
        0x60, 0x60, 0x60, 0x60, 0x66, 0x66, 0x6C, 0x6C, 0x78, 0x78, 0x6C, 0x6C, 0x66, 0x66, 0x00, 0x00,
        0x38, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x3C, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xEC, 0xEC, 0xFE, 0xFE, 0xD6, 0xD6, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x7C, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x3C, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x7C, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7C, 0x7C, 0x60, 0x60,
        0x00, 0x00, 0x00, 0x00, 0x3E, 0x3E, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x3E, 0x06, 0x06,
        0x00, 0x00, 0x00, 0x00, 0x7C, 0x7C, 0x66, 0x66, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x3E, 0x3E, 0x60, 0x60, 0x3C, 0x3C, 0x06, 0x06, 0x7C, 0x7C, 0x00, 0x00,
        0x00, 0x00, 0x18, 0x18, 0x7E, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x0E, 0x0E, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x3E, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xD6, 0xD6, 0x7C, 0x7C, 0x6C, 0x6C, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x3C, 0x3C, 0x18, 0x18, 0x3C, 0x3C, 0x66, 0x66, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x3E, 0x06, 0x06, 0x7C, 0x7C,
        0x00, 0x00, 0x00, 0x00, 0x7E, 0x7E, 0x0C, 0x0C, 0x18, 0x18, 0x30, 0x30, 0x7E, 0x7E, 0x00, 0x00,
        0x0E, 0x0E, 0x18, 0x18, 0x18, 0x18, 0x30, 0x30, 0x18, 0x18, 0x18, 0x18, 0x0E, 0x0E, 0x00, 0x00,
        0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00,
        0x70, 0x70, 0x18, 0x18, 0x18, 0x18, 0x0C, 0x0C, 0x18, 0x18, 0x18, 0x18, 0x70, 0x70, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0xF2, 0xF2, 0x9E, 0x9E, 0x0C, 0x0C, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    for (size_t i = 0; i < sizeof(tiles); i++) tiles[i] = ~tiles[i];
    //for (int i = 0; i < 6; i++) {
    //    for (int j = 0; j < 16; j++) {
    //        u8 *tile = tiles + i*16*16 + j*16;
    //        render_debug_tile(renderer, tile, j*tile_dim, i*tile_dim, tile_dim, tile_dim);
    //    }
    //}

    int tile_dim = 24;
    for (size_t i = 0; i < strlen(text); i++) {
        char c = text[i];
        //assert(c >= ' ' && c <= '~');
        if (c < ' ' || c > '~') c = '.';
        int index = c - ' ';
        int offset = index*16;
        u8 *tile = tiles + offset;
        render_debug_tile(renderer, tile, col*tile_dim+i*tile_dim, row*tile_dim, tile_dim, tile_dim);
    }
}

static void render_debug_hw_regs(GameBoy *gb, SDL_Renderer *renderer, int w, int h)
{
    (void)w;
    (void)h;
    int row = 2;
    int col = 2;
    char text[64];
    render_debug_text(renderer, "Misc.", row++, col);
    sprintf(text, "Type=%02X", gb->cart_type);
    render_debug_text(renderer, text, row++, col);
    sprintf(text, "JOYP=%02X", gb->memory[rP1]);
    render_debug_text(renderer, text, row++, col);

    row++;
    render_debug_text(renderer, "Int.", row++, col);
    sprintf(text, "IME =%02X", gb->IME);
    render_debug_text(renderer, text, row++, col);
    u8 intf = gb->memory[rIF];
    sprintf(text, "IF  =%02X %s%s%s%s%s", intf,
        intf & IEF_VBLANK ? "VBLANK " : "",
        intf & IEF_STAT   ? "STAT "   : "",
        intf & IEF_TIMER  ? "TIMER "  : "",
        intf & IEF_SERIAL ? "SERIAL " : "",
        intf & IEF_HILO   ? "JOYPAD " : "");
    render_debug_text(renderer, text, row++, col);
    u8 ie = gb->memory[rIE];
    sprintf(text, "IE  =%02X %s%s%s%s%s", ie,
        ie & IEF_VBLANK ? "VBLANK " : "",
        ie & IEF_STAT   ? "STAT "   : "",
        ie & IEF_TIMER  ? "TIMER "  : "",
        ie & IEF_SERIAL ? "SERIAL " : "",
        ie & IEF_HILO   ? "JOYPAD " : "");
    render_debug_text(renderer, text, row++, col);

    row++;
    render_debug_text(renderer, "Timer", row++, col);
    sprintf(text, "DIV =%02X", gb->memory[rDIV]);
    render_debug_text(renderer, text, row++, col);
    sprintf(text, "TIMA=%02X", gb->memory[rTIMA]);
    render_debug_text(renderer, text, row++, col);
    sprintf(text, "TMA =%02X", gb->memory[rTMA]);
    render_debug_text(renderer, text, row++, col);
    u8 tac = gb->memory[rTAC];
    if (tac & 4) {
        int freq[] = {4096, 262144, 65536, 16384};
        sprintf(text, "TAC =%02X %d Hz", tac, freq[tac & 3]);
    } else {
        sprintf(text, "TAC =%02X", tac);
    }
    render_debug_text(renderer, text, row++, col);

    row++;
    render_debug_text(renderer, "Display", row++, col);
    u8 lcdc = gb->memory[rLCDC];
    sprintf(text, "LCDC=%02X %s%s%s%s", lcdc,
        lcdc & LCDCF_ON    ? "LCD " : "",
        lcdc & LCDCF_WINON ? "WIN " : "",
        lcdc & LCDCF_OBJON ? "OBJ " : "",
        lcdc & LCDCF_BGON  ? "BG "  : ""
    );
    render_debug_text(renderer, text, row++, col);
    u8 stat = gb->memory[rSTAT];
    sprintf(text, "STAT=%02X %s", stat, stat & STATF_LYC ? "LYC" : "");
    render_debug_text(renderer, text, row++, col);
    sprintf(text, "SCX =%02X", gb->memory[rSCX]);
    render_debug_text(renderer, text, row++, col);
    sprintf(text, "SCY =%02X", gb->memory[rSCY]);
    render_debug_text(renderer, text, row++, col);
    sprintf(text, "LY  =%02X", gb->memory[rLY]);
    render_debug_text(renderer, text, row++, col);
    sprintf(text, "LYC =%02X", gb->memory[rLYC]);
    render_debug_text(renderer, text, row++, col);
    sprintf(text, "BGP =%02X", gb->memory[rBGP]);
    render_debug_text(renderer, text, row++, col);
    sprintf(text, "OBP0=%02X", gb->memory[rOBP0]);
    render_debug_text(renderer, text, row++, col);
    sprintf(text, "OBP1=%02X", gb->memory[rOBP1]);
    render_debug_text(renderer, text, row++, col);
    sprintf(text, "WX  =%02X", gb->memory[rWX]);
    render_debug_text(renderer, text, row++, col);
    sprintf(text, "WY  =%02X", gb->memory[rWY]);
    render_debug_text(renderer, text, row++, col);
    row += 1;
    sprintf(text, "Serial Index = %d", gb->serial_idx);
    render_debug_text(renderer, text, row++, col);
    sprintf(text, "Serial Data  =");
    render_debug_text(renderer, text, row++, col);
    sprintf(text, "%s", gb->serial_buffer);
    render_debug_text(renderer, text, row++, col);

    row = 2;
    col = 28;
    render_debug_text(renderer, "CPU Regs", row++, col);
    sprintf(text, "AF = %02X %02X", gb->AF >> 8, gb->AF & 0xFF);
    render_debug_text(renderer, text, row, col);
    sprintf(text, "%c%c%c%c",
        gb_get_flag(gb, Flag_Z) ? 'Z' : '-',
        gb_get_flag(gb, Flag_N) ? 'N' : '-',
        gb_get_flag(gb, Flag_H) ? 'H' : '-',
        gb_get_flag(gb, Flag_C) ? 'C' : '-');
    render_debug_text(renderer, text, row++, col + 12);

    sprintf(text, "BC = %02X %02X", gb_get_reg8(gb, REG_B), gb_get_reg8(gb, REG_C));
    render_debug_text(renderer, text, row++, col);

    sprintf(text, "DE = %02X %02X", gb_get_reg8(gb, REG_D), gb_get_reg8(gb, REG_E));
    render_debug_text(renderer, text, row++, col);

    sprintf(text, "HL = %04X", gb->HL);
    render_debug_text(renderer, text, row++, col);
    sprintf(text, "SP = %04X", gb->SP);
    render_debug_text(renderer, text, row++, col);
    sprintf(text, "PC = %04X", gb->PC);
    render_debug_text(renderer, text, row++, col);

    row++;
    render_debug_text(renderer, "Next inst", row++, col);
    Inst inst = gb_fetch(gb);
    char decoded[32];
    gb_decode(inst, decoded, sizeof(decoded));
    sprintf(text, "%04X: %s", gb->PC, decoded);
    render_debug_text(renderer, text, row++, col);

    if (inst.size == 1) sprintf(text, "%02X", inst.data[0]);
    else if (inst.size == 2) sprintf(text, "%02X %02X", inst.data[0], inst.data[1]);
    else if (inst.size == 3) sprintf(text, "%02X %02X %02X", inst.data[0], inst.data[1], inst.data[2]);
    render_debug_text(renderer, text, row++, col);
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

static void render_debug_viewport(GameBoy *gb, SDL_Renderer *renderer,
    int pixel_dim, int x, int y, int w, int h)
{
    int thick = 5;

    int wview = WIDTH*pixel_dim;
    int hview = HEIGHT*pixel_dim;

    int xleft   = x + gb->memory[rSCX]*pixel_dim;
    int xright  = xleft + wview;
    int ytop    = y + gb->memory[rSCY]*pixel_dim;
    int ybottom = ytop  + hview;

    SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(DBG_VIEW));

    // left
    SDL_Rect r = {xleft, ytop, thick, hview};
    SDL_RenderFillRect(renderer, &r);

    // right
    if (xright < w) {
        SDL_Rect r = {xright % w, ytop, thick, hview};
        SDL_RenderFillRect(renderer, &r);
    } else {
        SDL_Rect r = {x + (xright % w), ytop, thick, hview};
        SDL_RenderFillRect(renderer, &r);
    }

    int xextra = (xright < w)  ? 0 : (xright  % w);
    if (ybottom < h) {
        // top
        SDL_Rect r = {xleft, ytop, wview + thick - xextra, thick};
        SDL_RenderFillRect(renderer, &r);

        // bottom
        r = (SDL_Rect){xleft, ybottom % h, wview + thick - xextra, thick};
        SDL_RenderFillRect(renderer, &r);

        if (xextra > 0) {
            SDL_Rect r = {x, ytop, xextra + thick, thick};
            SDL_RenderFillRect(renderer, &r);

            r = (SDL_Rect){x, ybottom, xextra + thick, thick};
            SDL_RenderFillRect(renderer, &r);
        }
    } else {
        // top
        SDL_Rect r = {xleft, y + (ytop % h), wview + thick, thick};
        SDL_RenderFillRect(renderer, &r);

        // bottom
        r = (SDL_Rect){xleft, ybottom % h, wview + thick, thick};
        SDL_RenderFillRect(renderer, &r);

        if (xextra > 0) {
            SDL_Rect r = {x, y + (ytop % h), xextra + thick, thick};
            SDL_RenderFillRect(renderer, &r);

            r = (SDL_Rect){x, ybottom % h, xextra + thick, thick};
            SDL_RenderFillRect(renderer, &r);
        }
    }
}

static void render_debug_tilemap(GameBoy *gb, SDL_Renderer *renderer, int w, int h)
{
    int min_dim = h < w ? h : w;
    int pixel_dim = min_dim / 256; // GameBoy pixel size
    int x = (w - (256*pixel_dim))/2;
    int y = (h - (256*pixel_dim))/2;
    for (int row = 0; row < 256; row++) {
        for (int col = 0; col < 256; col++) {
            Color color = gb->display[row*256 + col];
            SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(color));
            SDL_Rect r = {x+col*pixel_dim, y+row*pixel_dim, pixel_dim, pixel_dim};
            SDL_RenderFillRect(renderer, &r);
        }
    }

    render_debug_viewport(gb, renderer, pixel_dim,
        x, y, x+255*pixel_dim, y+255*pixel_dim);
    render_debug_tile_grid(renderer, pixel_dim, x, y);
}

static void sdl_render(GameBoy *gb, SDL_Renderer *renderer)
{
    if (frame_ms < 16.0) return;

    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    if (viewer_type == VT_GAME) {
        if (gb->memory[rLY] != 144 && (gb->memory[rLCDC] & LCDCF_ON) != 0) return;

        SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(BG));
        SDL_RenderClear(renderer);

        int min_dim = h < w ? h : w;
        int pixel_dim = min_dim / 160; // GameBoy pixel size
        int x = (w - (160*pixel_dim))/2;
        int y = (h - (144*pixel_dim))/2;
        u16 scx = gb->memory[rSCX];
        u16 scy = gb->memory[rSCY];

        for (int row = 0; row < 144; row++) {
            u16 px_row = (row+scy) % 256;
            for (int col = 0; col < 160; col++) {
                u16 px_col = (col+scx) % 256;
                Color color = gb->display[px_row*256 + px_col];
                SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(color));
                SDL_Rect r = {
                    x+col*pixel_dim, y+row*pixel_dim,
                    pixel_dim, pixel_dim};
                SDL_RenderFillRect(renderer, &r);
            }
        }

        frame_ms -= 16.0;
        SDL_RenderPresent(renderer);
    } else {
        // Debug rendering
        if (viewer_type != VT_REGS &&
            gb->memory[rLY] != 144 && (gb->memory[rLCDC] & LCDCF_ON) != 0) return;

        SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(BG));
        SDL_RenderClear(renderer);

        if (viewer_type == VT_TILEMAP) {
            render_debug_tilemap(gb, renderer, w, h);
        } else if (viewer_type == VT_TILES) {
            render_debug_tiles(renderer, w, h, gb->memory + 0x8000);
        } else if (viewer_type == VT_REGS) {
            render_debug_hw_regs(gb, renderer, w, h);
        }

        frame_ms -= 16.0;
        SDL_RenderPresent(renderer);
    }
}

static bool cstr_ends_with(const char *src, const char *end) {
    long src_len = strlen(src);
    long end_len = strlen(end);
    if (end_len > src_len) return false;
    for (size_t i = 0; i < (size_t)end_len; i++) {
        if (src[src_len - i] != end[end_len - i]) return false;
    }
    return true;
}

static void sdl_init(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
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

    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        fprintf(stderr, "Failed to create renderer\n");
        exit(1);
    }

    SDL_AudioSpec audio_spec;
    int sample_rate = SAMPLE_RATE;
    SDL_AudioSpec desired_spec = {
        .freq = sample_rate,
        .format = AUDIO_F32LSB,
        .samples = 1024,
        .channels = 1,
    };
    device = SDL_OpenAudioDevice(NULL, 0, &desired_spec, &audio_spec, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (!device) {
        fprintf(stderr, "Failed to open audio device\n");
        exit(1);
    }
}

static void play_square_wave(int freq, int ms, int duty_cycle)
{
    // 46.875 ms => How many samples ?
    int samples_per_sweep = 2250;

    // 48000 samples    => 1 s      => 1000 ms
    //   120 samples    => 1/400 s  => 1000/400 ms
    int samples_per_cycle = (f32)SAMPLE_RATE / (f32)freq;
    int samples_high;
    switch (duty_cycle) {
        case 0:
            samples_high = samples_per_cycle * 0.875;
            break;
        case 1:
            samples_high = samples_per_cycle * 0.75;
            break;
        case 2:
            samples_high = samples_per_cycle * 0.5;
            break;
        case 3:
            samples_high = samples_per_cycle * 0.25;
            break;
        default: assert(0 && "Duty cycle mode not supported");
    }

    int sample_count = (ms*SAMPLE_RATE) / 1000;
    assert(sample_count < 48000);
    f32 samples[48000] = {0};
    for (int i = 0; i < sample_count; i++) {
        f32 sample = (i % samples_per_cycle) < samples_high ? 1.0f : 0.0f;
        sample *= VOLUME;

        int sweep_index = i / samples_per_sweep;
        sample *= ((16.0f - sweep_index) / 16.0f);

        samples[i] = sample;
    }

    SDL_QueueAudio(device, samples, sample_count * sizeof(samples[0]));

    SDL_PauseAudioDevice(device, 0);
}

static void tile_viewer(const char *file_path)
{
    size_t size;
    u8 *tile_data = read_entire_file(file_path, &size);
    printf("Tiledata size: %ld\n", size);

    SDL_Event e;
    bool running = true;
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                SDL_Keycode key = e.key.keysym.sym;
                if (key == SDLK_ESCAPE) running = false;
            }
        }

        int w, h;
        SDL_GetWindowSize(window, &w, &h);

        SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(BG));
        SDL_RenderClear(renderer);

        render_debug_tiles(renderer, w, h, tile_data);

        SDL_RenderPresent(renderer);
    }
}

static void sdl_process_events(GameBoy *gb)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            gb->running = false;
        } else if (e.type == SDL_WINDOWEVENT) {
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                if (e.window.data1 < 256 || e.window.data2 < 256) {
                    //SDL_SetWindowSize(window, 256, 256);
                }
            }
        } else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
            switch (e.key.keysym.sym) {
            case SDLK_ESCAPE:
                gb->running = false;
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
            case SDLK_r:
                viewer_type = VT_REGS;
                break;
            case SDLK_p:
                if (e.key.type == SDL_KEYDOWN) {
                    gb->paused = !gb->paused;
                }
                break;
            case SDLK_1:
                if (e.key.type == SDL_KEYDOWN) play_square_wave(1048, 100, 2);
                break;
            case SDLK_2:
                if (e.key.type == SDL_KEYDOWN) play_square_wave(2080, 900, 2);
                break;
            case SDLK_3:
                if (e.key.type == SDL_KEYDOWN) play_square_wave(440, 100, 2);
                break;
            case SDLK_4:
                if (e.key.type == SDL_KEYDOWN) play_square_wave(440, 100, 3);
                break;
            case SDLK_SPACE:
                if (e.key.type == SDL_KEYDOWN) {
                    gb_dump(gb);
                    printf("BG tilemap $9800-$9BFF:\n");
                    for (int row = 0; row < 32; row++) {
                        for (int col = 0; col < 32; col++) {
                            printf("%02X ", gb->memory[0x9800 + row*32 + col]);
                        }
                        printf("\n");
                    }
                    printf("\n");

                    printf("BG tilemap $9C00-$9FFF:\n");
                    for (int row = 0; row < 32; row++) {
                        for (int col = 0; col < 32; col++) {
                            printf("%02X ", gb->memory[0x9C00 + row*32 + col]);
                        }
                        printf("\n");
                    }
                    printf("\n");

                    printf("OAM $FE00-$FE9F:\n");
                    for (int i = 0; i < 40; i++) {
                        u8 y = gb->memory[_OAMRAM + i*4 + 0] - 16;
                        u8 x = gb->memory[_OAMRAM + i*4 + 1] - 8;
                        u8 tile_idx = gb->memory[_OAMRAM + i*4 + 2];
                        u8 attribs = gb->memory[_OAMRAM + i*4 + 3];
                        printf("X: %3d, Y: %3d, Tile: %3d (%02X), Attrib: %02X\n",
                            x, y, tile_idx, tile_idx, attribs);
                    }

                    printf("$FFA4: %02X\n", gb->memory[0xFFA4]);
                    printf("SCX: %02X\n", gb->memory[rSCX]);
                }
                break;
            case SDLK_s:
                gb->button_a = e.key.type == SDL_KEYDOWN ? 1 : 0;
                break;
            case SDLK_a:
                gb->button_b = e.key.type == SDL_KEYDOWN ? 1 : 0;
                break;
            case SDLK_RETURN:
                gb->button_start = e.key.type == SDL_KEYDOWN ? 1 : 0;
                break;
            case SDLK_LSHIFT:
                gb->button_select = e.key.type == SDL_KEYDOWN ? 1 : 0;
                break;
            case SDLK_UP:
                gb->dpad_up = e.key.type == SDL_KEYDOWN ? 1 : 0;
                break;
            case SDLK_DOWN:
                gb->dpad_down = e.key.type == SDL_KEYDOWN ? 1 : 0;
                break;
            case SDLK_RIGHT:
                gb->dpad_right = e.key.type == SDL_KEYDOWN ? 1 : 0;
                break;
            case SDLK_LEFT:
                gb->dpad_left = e.key.type == SDL_KEYDOWN ? 1 : 0;
                break;

            // Debug hotkeys
            case SDLK_d:
                break;
            case SDLK_n:
                break;
            default:
                break;
            }
        }
    }
}

void emulator(int argc, char **argv)
{
    GameBoy gb = {0};
    gb_init_with_args(&gb, argc, argv);

    Uint64 counter_freq  = SDL_GetPerformanceFrequency();
    Uint64 start_counter = SDL_GetPerformanceCounter();
    Uint64 prev_counter  = start_counter;
    while (gb.running) {
        Uint64 curr_counter = SDL_GetPerformanceCounter();
        Uint64 dt_counter = curr_counter - prev_counter;
        prev_counter = curr_counter;
        f64 dt_ms = (f64)(1000.0 * dt_counter) / counter_freq;
        frame_ms += dt_ms;

        sdl_process_events(&gb);

        gb_update(&gb);

        sdl_render(&gb, renderer);
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <path to ROM>\n", argv[0]);
        exit(1);
    }

    sdl_init();

    if (cstr_ends_with(argv[1], ".2bpp")) {
        printf("Displaying tile data (.2bpp)\n");
        tile_viewer(argv[1]);
    }

    if (cstr_ends_with(argv[1], ".gb")) {
        emulator(argc, argv);
    }

    return 0;
}
