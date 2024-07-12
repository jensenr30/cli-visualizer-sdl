/*
 * NcursesWriter.cpp
 *
 * Created on: Jul 30, 2015
 *     Author: dpayne
 */
#include <cmath>

#include "Domain/VisConstants.h"
#include "Utils/Logger.h"
#include "Utils/NcursesUtils.h"
#include "Writer/NcursesWriter.h"

#include <SDL2/SDL.h>

#ifdef _LINUX
/* Ncurses version 6.0.20170401 introduced an issue with COLOR_PAIR which broke
 * setting more than 256 color pairs. Specifically it uses an A_COLOR macro
 * which uses a 8 bit mask. This will work for colors since only 256 colors are
 * supported but it breaks color pairs since 2^16 color pairs are supported.
 */
#define VIS_A_COLOR (NCURSES_BITS(((1U) << 16) - 1U, 0))
#define VIS_COLOR_PAIR(n) (NCURSES_BITS((n), 0) & VIS_A_COLOR)
#else
#define VIS_COLOR_PAIR(n) (COLOR_PAIR(n))
#endif

void exit_msg(const char *msg) {
    printf("%s", msg);
    exit(1);
}

SDL_Surface *screen;
SDL_Renderer *renderer;
SDL_Texture *screen_texture;
static uint32_t SCREEN_WIDTH;
static uint32_t SCREEN_HEIGHT;

static void handle_window_resize_event(uint32_t width, uint32_t height) {
    printf("width %d, height %d\n", width, height);
    SCREEN_WIDTH = width;
    SCREEN_HEIGHT = height;

    if (screen != nullptr) {
        SDL_FreeSurface(screen);
    }
    screen = SDL_CreateRGBSurfaceWithFormat(
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        32,
        SDL_PIXELFORMAT_ARGB8888);

    if (screen_texture != nullptr) {
        SDL_DestroyTexture(screen_texture);
    }
    screen_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_WIDTH,
        SCREEN_HEIGHT);
}

vis::NcursesWriter::NcursesWriter()
{
    initscr();
    noecho();    // disable printing of pressed keys
    curs_set(0); // sets the cursor to invisible
    setlocale(LC_ALL, VisConstants::k_default_locale.c_str());

    if (static_cast<int>(has_colors()) == TRUE)
    {
        start_color();        // turns on color
        use_default_colors(); // uses default colors of terminal, which allows
                              // transparency to work
    }

    // setup SDL
    uint32_t flags = 0;
    #ifdef __linux__
        flags = SDL_INIT_EVERYTHING;
    #elif __MINGW32__
        // specifically for wine otherwise im pretty sure SDL_INIT_EVERYTHING works on normal windows
        // SDL_INIT_SENSOR doesn't seem to work on wine right now
        flags = SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMECONTROLLER;
    #elif __EMSCRIPTEN__
        flags = SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS;
    #else
        exit_msg("no flags supported yet");
    #endif
    if(SDL_Init(flags) == -1) {
        exit_msg("Could not init SDL");
    }

    // setup SDL window
    SDL_Window *window = SDL_CreateWindow("TileVenture", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if(window == nullptr) {
        exit_msg("Could not init SDL Window");
    }

    // create renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        exit_msg("Could not init renderer\n");
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    handle_window_resize_event(720, 620);
}


void vis::NcursesWriter::SDL_Loop()
{
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        if(event.type == SDL_QUIT) {
            exit_msg("sdl_quit\n");
        } else if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                exit_msg("window quit!\n");
            } else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                printf("window resize event\n");
                handle_window_resize_event(event.window.data1, event.window.data2);
            }
        }
    }

    // printf("update texture\n");
    // SDL_Rect rect = {.x = 0, .y = 0, .w = 100, .h = 100};
    // SDL_FillRect(screen, &rect, 0xffffffff);
    // update screen texture
    SDL_UpdateTexture(screen_texture, nullptr, screen->pixels, screen->pitch);
    // clear renderer
    SDL_RenderClear(renderer);
    // // copy image into renderer to be rendered
    SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
    // present that shit
    SDL_RenderPresent(renderer);
}

void vis::NcursesWriter::setup_color_pairs(
    bool is_override_terminal_colors,
    const std::vector<ColorDefinition> &colors)
{
    // initialize colors
    for (const auto &color : colors)
    {
        if (is_override_terminal_colors && color.get_red() >= 0)
        {
            init_color(color.get_color_index(), color.get_red(),
                       color.get_green(), color.get_blue());
        }

        init_pair(color.get_color_index(), color.get_color_index(), -1);

        // initialize colors as background, this is used in write_background to
        // create a
        // full block effect without using a custom font
        init_pair(
            static_cast<int16_t>(color.get_color_index() +
                                 NcursesUtils::number_of_colors_supported()),
            color.get_color_index(), color.get_color_index());
    }
}

void vis::NcursesWriter::setup_colors(
    bool is_override_terminal_colors,
    const std::vector<ColorDefinition> &colors)
{
    if (static_cast<int>(has_colors()) == TRUE)
    {
        start_color();        // turns on color
        use_default_colors(); // uses default colors of terminal, which allows
                              // transparency to work

        // only supports max 256 colors
        setup_color_pairs(is_override_terminal_colors, colors);
    }
}

void vis::NcursesWriter::write_background(int32_t height, int32_t width,
                                          vis::ColorDefinition color,
                                          const std::wstring &msg)
{
    // Add COLORS which will set it to have the color as the background, see
    // "setup_colors"
    attron(VIS_COLOR_PAIR(color.get_color_index() +
                          NcursesUtils::number_of_colors_supported()));

    mvaddwstr(height, width, msg.c_str());

    attroff(VIS_COLOR_PAIR(color.get_color_index() +
                           NcursesUtils::number_of_colors_supported()));
}

void vis::NcursesWriter::write_foreground(int32_t height, int32_t width,
                                          vis::ColorDefinition color,
                                          const std::wstring &msg)
{
    attron(VIS_COLOR_PAIR(color.get_color_index()));

    mvaddwstr(height, width, msg.c_str());

    attroff(VIS_COLOR_PAIR(color.get_color_index()));
}

void vis::NcursesWriter::write(const double row, const double column,
                               const vis::ColorDefinition color,
                               const std::wstring &msg, const wchar_t character)
{
    // This is a hack to achieve a solid bar look without using a custom font.
    // Instead of writing a real character, set the background to the color and
    // write a space
    // if (character == VisConstants::k_space_wchar)
    // {
    //     write_background(row, column, color, msg);
    // }
    // else
    // {
    //     write_foreground(row, column, color, msg);
    // }
    int32_t terminal_bar_width_in_chars = msg.size();
    int terminal_height = NcursesUtils::get_window_height();
    int terminal_width = NcursesUtils::get_window_width();
    int vertical_scale_factor = SCREEN_HEIGHT / terminal_height;
    int horizontal_scale_factor = SCREEN_WIDTH / terminal_width;

    SDL_Rect rect;
    rect.x = (double)horizontal_scale_factor*column;
    rect.y = (double)vertical_scale_factor*row;
    rect.w = horizontal_scale_factor*terminal_bar_width_in_chars;
    rect.h = vertical_scale_factor + 1;

    uint8_t a = 0xff;
    uint8_t r = color.get_red();
    uint8_t g = color.get_green();
    uint8_t b = color.get_blue();
    uint32_t color_argb = a << 24 | r << 16 | g << 8 | b;
    SDL_FillRect(screen, &rect, color_argb);
    // printf("row %d, column %d\n", row, column);
}

void vis::NcursesWriter::clear()
{
    standend();
    erase();
    SDL_FillRect(screen, nullptr, 0x000000);
}

void vis::NcursesWriter::flush()
{
    refresh();
}

vis::ColorDefinition
vis::NcursesWriter::to_color_pair(int32_t number, int32_t max,
                                  std::vector<ColorDefinition> colors,
                                  bool wrap) const
{
    const auto colors_size = static_cast<vis::ColorIndex>(colors.size());
    const auto index = (number * colors_size) / (max + 1);

    // no colors
    if (colors_size == 0)
    {
        return vis::ColorDefinition{0, 0, 0, 0};
    }

    const auto color = colors[static_cast<size_t>(
        wrap ? index % colors_size : std::min(index, colors_size - 1))];

    return color;
}

vis::NcursesWriter::~NcursesWriter()
{
    endwin();
}
