#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "gb.h"

#define SCALE  5

#define TILEMAP_ROWS 32
#define TILEMAP_COLS 32
#define VIEWPORT_COLS 20
#define VIEWPORT_ROWS 18
#define TILE_PIXELS 8
#define OAM_COUNT 40

#define CPU_FREQ    4194304.0 // 4.19 MHz
#define VSYNC       59.73
#define HSYNC       9198.0    // 9.198 KHz

#define CLOCK_MS    (1000.0 / CPU_FREQ)
#define VSYNC_MS    (1000.0 / VSYNC)
#define HSYNC_MS    (1000.0 / HSYNC)

#define VRAM_TILES      0x8000
//#define VRAM_TILES      0x9000
#define VRAM_TILEMAP    0x9800

#define rP1     0xFF00
#define rLCDC   0xFF40
#define rLY     0xFF44
#define rBGP    0xFF47
#define rWY     0xFF4A
#define rWX     0xFF4B

#define LCDCF_ON        0x80
#define LCDCF_WIN9C00   0x40
#define LCDCF_WINON     0x20
#define LCDCF_BG8000    0x10
#define LCDCF_OBJON     0x02
#define LCDCF_BGON      0x01

#define P1F_GET_BTN  0x10
#define P1F_GET_DPAD 0x20
#define P1F_GET_NONE (P1F_GET_BTN | P1F_GET_DPAD)

const uint8_t NINTENDO_LOGO[] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
};

// Tile: 8x8 pixels, 2 bpp (index into Palette)
const uint8_t PALETTE[4] = {0x00, 0x40, 0x80, 0xFF};
//const uint8_t PALETTE[4] = {0xff, 0x80, 0x40, 0x00};

#define TILE_SIZE 16

// Debug Status
static bool step_debug;

void gb_render_tile(GameBoy *gb, SDL_Renderer *renderer, const uint8_t *tile, int xoffset, int yoffset, bool transparency)
{
    uint8_t bgp = gb->memory[rBGP]; // E4 - 11|10|01|00
    uint8_t bgp_tbl[] = {bgp >> 6, (bgp >> 4) & 3, (bgp >> 2) & 3, bgp & 3};

    for (int tile_row = 0; tile_row < TILE_PIXELS; tile_row++) {
        uint8_t low_bitplane = tile[tile_row*2+0];
        uint8_t high_bitplane = tile[tile_row*2+1];
        for (int tile_col = 0; tile_col < TILE_PIXELS; tile_col++) {
            uint8_t bit0 = (low_bitplane & 0x80) >> 7;
            uint8_t bit1 = (high_bitplane & 0x80) >> 7;
            low_bitplane <<= 1;
            high_bitplane <<= 1;

            uint8_t color_idx = (bit1 << 1) | bit0;
            uint8_t color = PALETTE[bgp_tbl[color_idx]];

            if (!transparency || color_idx != 0) {
                SDL_SetRenderDrawColor(renderer, color, color, color, 255);
                SDL_Rect rect = {.x = xoffset + tile_col*SCALE, .y = yoffset + tile_row*SCALE, .w = SCALE, .h = SCALE};
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }
}

bool gb_render(GameBoy *gb, SDL_Renderer *renderer)
{
    if ((gb->memory[rLCDC] & LCDCF_ON  /*0x80*/) == 0) return false;
    if ((gb->memory[rLCDC] & LCDCF_BGON/*0x01*/) == 0) return false;

    // Render the BG
    bool bg8000_mode = (gb->memory[rLCDC] & LCDCF_BG8000/*0x10*/);
    uint16_t bg_win_tile_data_offset = bg8000_mode ? 0x8000 : 0x9000;

    int yoffset = 0;
    int xoffset = 0;
    for (int row = 0; row < TILEMAP_ROWS/*VIEWPORT_ROWS*/; row++) {
        for (int col = 0; col < TILEMAP_COLS/*VIEWPORT_COLS*/; col++) {
            int tile_idx = bg8000_mode ?
                gb->memory[VRAM_TILEMAP + row*(TILEMAP_COLS) + col] :
                (int8_t)gb->memory[VRAM_TILEMAP + row*(TILEMAP_COLS) + col];

            const uint8_t *tile = gb->memory + bg_win_tile_data_offset + tile_idx*TILE_SIZE;
            gb_render_tile(gb, renderer, tile, xoffset, yoffset, false);
            xoffset += TILE_PIXELS*SCALE;
        }
        yoffset += TILE_PIXELS*SCALE;
        xoffset = 0;
    }

    // Render Window
    if ((gb->memory[rLCDC] & LCDCF_WINON/*0x20*/) != 0) {
        uint16_t win_tile_data_offset = (gb->memory[rLCDC] & LCDCF_WIN9C00/*0x40*/) ? 0x9C00 : 0x9800;
        int win_x = gb->memory[rWX] - 7;
        int win_y = gb->memory[rWY];
        yoffset = win_y*SCALE;
        xoffset = win_x*SCALE;

        for (int row = 0; row < VIEWPORT_ROWS; row++) {
            for (int col = 0; col < VIEWPORT_COLS; col++) {
                int tile_idx = gb->memory[win_tile_data_offset + row*(TILEMAP_COLS) + col];
                const uint8_t *tile = gb->memory + bg_win_tile_data_offset + tile_idx*TILE_SIZE;
                gb_render_tile(gb, renderer, tile, xoffset, yoffset, false);
                xoffset += TILE_PIXELS*SCALE;
            }
            yoffset += TILE_PIXELS*SCALE;
            xoffset = win_x*SCALE;
        }
    }

    // Render OBJ
    if ((gb->memory[rLCDC] & LCDCF_OBJON/*0x02*/) != 0) {
        for (int i = 0; i < OAM_COUNT; i++) {
            uint8_t *obj = gb->memory + 0xFE00 + i*4;
            int y = *(obj+0) - 16;
            int x = *(obj+1) - 8;
            int tile_id = *(obj+2);
            const uint8_t *tile = gb->memory + 0x8000 + tile_id*TILE_SIZE;
            gb_render_tile(gb, renderer, tile, x*SCALE, y*SCALE, true);
            //printf("OBJ[%2d] x: %d, y: %d, tileID: %d, attributes: 0x%02X\n",
            //    i, x, y, tileID, *(obj+3));
        }
    }

    return true;
}

void gb_render_logo(SDL_Renderer *renderer, int width, int height)
{
    // The Logo is 48x8 pixels
    int pixel_width = (width/2) / (12*4);
    int pixel_height = (height/2) / 8;
    int xstart = (width/2) - (24*pixel_width);
    int ystart = (height/2) - (4*pixel_height);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    int xoffset = xstart;
    int yoffset = ystart;
    for (int i = 0; i < 24; i++) {
        uint8_t b = NINTENDO_LOGO[i];
        uint8_t first_row = b >> 4;
        uint8_t second_row = b & 0xf;

        for (int j = 0; j < 4; j++) {
            if ((first_row & 0x8) != 0) {
                SDL_Rect rect = {.x = xoffset + j*pixel_width, .y = yoffset, .w = pixel_width, .h = pixel_height};
                SDL_RenderFillRect(renderer, &rect);
            }
            first_row <<= 1;
        }
        for (int j = 0; j < 4; j++) {
            if ((second_row & 0x8) != 0) {
                SDL_Rect rect = {.x = xoffset + j*pixel_width, .y = yoffset + pixel_height, .w = pixel_width, .h = pixel_height};
                SDL_RenderFillRect(renderer, &rect);
            }
            second_row <<= 1;
        }

        yoffset = yoffset + 2*pixel_height;
        if (i & 0x1) {
            yoffset = ystart;
            xoffset = xoffset + 4*pixel_width;
        }
    }

    // Second half
    xoffset = xstart;
    yoffset = ystart + pixel_height*4;
    for (int i = 24; i < 48; i++) {
        uint8_t b = NINTENDO_LOGO[i];
        uint8_t first_row = b >> 4;
        uint8_t second_row = b & 0xf;

        for (int j = 0; j < 4; j++) {
            if ((first_row & 0x8) != 0) {
                SDL_Rect rect = {.x = xoffset + j*pixel_width, .y = yoffset, .w = pixel_width, .h = pixel_height};
                SDL_RenderFillRect(renderer, &rect);
            }
            first_row <<= 1;
        }
        for (int j = 0; j < 4; j++) {
            if ((second_row & 0x8) != 0) {
                SDL_Rect rect = {.x = xoffset + j*pixel_width, .y = yoffset + pixel_height, .w = pixel_width, .h = pixel_height};
                SDL_RenderFillRect(renderer, &rect);
            }
            second_row <<= 1;
        }

        yoffset = yoffset + 2*pixel_height;
        if (i & 0x1) {
            yoffset = ystart + pixel_height*4;
            xoffset = xoffset + 4*pixel_width;
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <path to ROM>\n", argv[0]);
        exit(1);
    }

    bool show_tile_grid = false;

    step_debug = true;
    GameBoy gb = {0};
    gb_load_rom_file(&gb, argv[1]);

    //uint8_t buf[] = {0xEA, 0x94, 0xFF};
    //Inst inst = {.data = buf, .size = 3};
    //gb_exec(&gb, inst);
    //exit(1);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Failed to initialize SDL\n");
        exit(1);
    }

    int width = SCALE*WIDTH; // + SCALE*WIDTH;
    int height = SCALE*HEIGHT; // + SCALE*HEIGHT;
    SDL_Window *window = SDL_CreateWindow("GameBoy Emulator", 0, 0, width, height, 0);
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
    bool running = true;
    Uint64 counter_freq = SDL_GetPerformanceFrequency();
    printf("Performance Frequency: %ld\n", counter_freq);
    Uint64 start_counter = SDL_GetPerformanceCounter();
    Uint64 prev_counter = start_counter;
    while (running) {
        Uint64 curr_counter = SDL_GetPerformanceCounter();
        Uint64 delta_counter = curr_counter - prev_counter;
        prev_counter = curr_counter;
        double dt_ms = (double)delta_counter / (counter_freq / 1000.0);
        //printf("dt: %f ms\n", dt_ms);

        // Input
        // TODO: fix unresponsiveness
        SDL_PollEvent(&e);
        if (e.type == SDL_QUIT) {
            running = false;
        } else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
            switch (e.key.keysym.sym) {
                case SDLK_ESCAPE:
                    running = false;
                    break;
                case SDLK_g:
                    if (e.key.type == SDL_KEYDOWN) {
                        show_tile_grid = !show_tile_grid;
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

        // Update
        for (int i = 0; i < 20; i++) {
            gb_tick(&gb, dt_ms);
        }

        // Render
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (gb_render(&gb, renderer)) {
            //gb_render_logo(renderer, width, height);

            if (show_tile_grid) {
                SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
                for (int i = 0; i <= 32; i++) {
                    int y = i*TILE_PIXELS*SCALE;
                    SDL_RenderDrawLine(renderer, 0, y, 32*TILE_PIXELS*SCALE, y);
                }
                for (int i = 0; i <= 32; i++) {
                    int x = i*TILE_PIXELS*SCALE;
                    SDL_RenderDrawLine(renderer, x, 0, x, 32*TILE_PIXELS*SCALE);
                }
            }

            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
            int thickness = 5;
            SDL_Rect r = {0, HEIGHT*SCALE, WIDTH*SCALE, thickness};
            SDL_RenderFillRect(renderer, &r);
            r = (SDL_Rect){WIDTH*SCALE, 0, thickness, HEIGHT*SCALE};
            SDL_RenderFillRect(renderer, &r);

            SDL_RenderPresent(renderer);
        }
    }

    return 0;
}
