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

#define BG          0x131313FF
#define DBG_GRID    0x00C400FF
#define DBG_VIEW    0xFFFF14FF

static SDL_Window *window;
static bool show_tile_grid = false;

static void render_debug_tile_grid(SDL_Renderer *renderer, int pixel_dim, int xstart, int ystart)
{
    if (show_tile_grid) {
        SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(DBG_GRID));
        for (int i = 0; i <= 32; i++) {
            int y = ystart+i*TILE_PIXELS*pixel_dim;
            SDL_RenderDrawLine(renderer,
                xstart+0, y,
                xstart+32*TILE_PIXELS*pixel_dim, y);
        }
        for (int i = 0; i <= 32; i++) {
            int x = xstart+i*TILE_PIXELS*pixel_dim;
            SDL_RenderDrawLine(renderer,
                x, ystart+0,
                x, ystart+32*TILE_PIXELS*pixel_dim);
        }
    }
}

static void render_debug_viewport(SDL_Renderer *renderer, int pixel_dim, int xstart, int ystart)
{
    SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(DBG_VIEW));
    int thickness = 5;
    SDL_Rect r = {
        xstart+0, ystart+HEIGHT*pixel_dim,
        WIDTH*pixel_dim+thickness, thickness};
    SDL_RenderFillRect(renderer, &r);
    r = (SDL_Rect){
        xstart+WIDTH*pixel_dim, ystart+0,
        thickness, HEIGHT*pixel_dim};
    SDL_RenderFillRect(renderer, &r);
}

void sdl_render(GameBoy *gb, SDL_Renderer *renderer)
{
    if (gb->memory[rLCDC] == 0) return;

    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(BG));
    SDL_RenderClear(renderer);

    int min_dim = h < w ? h : w;
    int pixel_dim = min_dim / 256; // GameBoy pixel size

    int xstart = (w - (256*pixel_dim))/2;
    int ystart = (h - (256*pixel_dim))/2;

    for (int row = 0; row < 256; row++) {
        for (int col = 0; col < 256; col++) {
            uint8_t color = gb->display[row*256 + col];
            //SDL_SetRenderDrawColor(renderer, HEX_TO_COLOR(RED));
            SDL_SetRenderDrawColor(renderer, color, color, color, 255);
            SDL_Rect r = {
                xstart+col*pixel_dim, ystart+row*pixel_dim,
                pixel_dim, pixel_dim};
            SDL_RenderFillRect(renderer, &r);
        }
    }

    // Debug rendering
    render_debug_tile_grid(renderer, pixel_dim, xstart, ystart);
    render_debug_viewport(renderer, pixel_dim, xstart, ystart);

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
    window = SDL_CreateWindow("GameBoy Emulator", 0, 0, width, height, SDL_WINDOW_RESIZABLE);
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
    while (gb.running) {
        Uint64 curr_counter = SDL_GetPerformanceCounter();
        Uint64 delta_counter = curr_counter - prev_counter;
        prev_counter = curr_counter;
        double dt_ms = (double)delta_counter / (counter_freq / 1000.0);
        //printf("dt: %f ms\n", dt_ms);

        // Input
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                gb.running = false;
            } else if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    if (e.window.data1 < 256 || e.window.data2 < 256) {
                        SDL_SetWindowSize(window, 256, 256);
                    }
                }
            } else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                switch (e.key.keysym.sym) {
                case SDLK_ESCAPE:
                    gb.running = false;
                    break;
                case SDLK_g:
                    if (e.key.type == SDL_KEYDOWN) {
                        show_tile_grid = !show_tile_grid;
                    }
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

        // Update
        //for (int i = 0; i < 20; i++) {
            gb_tick(&gb, dt_ms);
        //}

        // Render
        sdl_render(&gb, renderer);
    }

    return 0;
}
