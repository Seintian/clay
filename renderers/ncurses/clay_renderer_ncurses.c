#include <ncurses.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../clay.h"

// -------------------------------------------------------------------------------------------------
// -- Internal State & Context
// -------------------------------------------------------------------------------------------------

#define CLAY_NCURSES_CELL_WIDTH 8.0f
#define CLAY_NCURSES_CELL_HEIGHT 16.0f

static int _clayNcursesScreenWidth = 0;
static int _clayNcursesScreenHeight = 0;
static bool _clayNcursesInitialized = false;

// Scissor / Clipping State
#define MAX_SCISSOR_STACK_DEPTH 16
static Clay_BoundingBox _scissorStack[MAX_SCISSOR_STACK_DEPTH];
static int _scissorStackIndex = 0;

// Color State
// We reserve pair 0. Pairs 1..max are dynamically allocated.
#define MAX_COLOR_PAIRS_CACHE 256
static struct {
    short fg;
    short bg;
    int pairId;
} _colorPairCache[MAX_COLOR_PAIRS_CACHE];
static int _colorPairCacheSize = 0;

// -------------------------------------------------------------------------------------------------
// -- Constants
// -------------------------------------------------------------------------------------------------

// Standard ANSI Colors mapped to easier indices if needed, 
// allows extending to 256 colors easily later.

// -------------------------------------------------------------------------------------------------
// -- Forward Declarations
// -------------------------------------------------------------------------------------------------

static short Clay_Ncurses_GetColorId(Clay_Color color);
static int Clay_Ncurses_GetColorPair(short fg, short bg);
static bool Clay_Ncurses_IntersectScissor(int x, int y, int w, int h, int *outX, int *outY, int *outW, int *outH);

// -------------------------------------------------------------------------------------------------
// -- Public API Implementation
// -------------------------------------------------------------------------------------------------

void Clay_Ncurses_Initialize() {
    if (_clayNcursesInitialized) return;

    initscr();
    cbreak(); // Line buffering disabled
    noecho(); // Don't echo input
    keypad(stdscr, TRUE); // Enable arrow keys
    curs_set(0); // Hide cursor

    // Enable mouse events if available
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);

    start_color();
    use_default_colors();

    // Refresh screen dimensions
    getmaxyx(stdscr, _clayNcursesScreenHeight, _clayNcursesScreenWidth);

    // Initialize Scissor Stack with full screen
    _scissorStack[0] = (Clay_BoundingBox){0, 0, (float)_clayNcursesScreenWidth * CLAY_NCURSES_CELL_WIDTH, (float)_clayNcursesScreenHeight * CLAY_NCURSES_CELL_HEIGHT};
    _scissorStackIndex = 0;

    _clayNcursesInitialized = true;
}

void Clay_Ncurses_Terminate() {
    if (_clayNcursesInitialized) {
        clear();
        refresh();
        endwin();
        _clayNcursesInitialized = false;
    }
}

Clay_Dimensions Clay_Ncurses_GetLayoutDimensions() {
    return (Clay_Dimensions) {
        .width = (float)_clayNcursesScreenWidth * CLAY_NCURSES_CELL_WIDTH,
        .height = (float)_clayNcursesScreenHeight * CLAY_NCURSES_CELL_HEIGHT
    };
}

Clay_Dimensions Clay_Ncurses_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    (void)config;
    (void)userData;
    // Simple 1-to-1 mapping
    return (Clay_Dimensions) {
        .width = (float)text.length * CLAY_NCURSES_CELL_WIDTH,
        .height = CLAY_NCURSES_CELL_HEIGHT
    };
}

void Clay_Ncurses_Render(Clay_RenderCommandArray renderCommands) {
    if (!_clayNcursesInitialized) return;

    erase(); // Clear buffer

    // Update dimensions on render start (handle resize gracefully-ish)
    int newW, newH;
    getmaxyx(stdscr, newH, newW);
    if (newW != _clayNcursesScreenWidth || newH != _clayNcursesScreenHeight) {
        _clayNcursesScreenWidth = newW;
        _clayNcursesScreenHeight = newH;
    }

    // Reset Scissor Stack
    _scissorStack[0] = (Clay_BoundingBox){0, 0, (float)_clayNcursesScreenWidth * CLAY_NCURSES_CELL_WIDTH, (float)_clayNcursesScreenHeight * CLAY_NCURSES_CELL_HEIGHT};
    _scissorStackIndex = 0;

    for (int i = 0; i < renderCommands.length; i++) {
        Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&renderCommands, i);
        Clay_BoundingBox box = command->boundingBox;

        switch (command->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                // Convert to integer coords
                int x = (int)(box.x / CLAY_NCURSES_CELL_WIDTH);
                int y = (int)(box.y / CLAY_NCURSES_CELL_HEIGHT);
                int w = (int)(box.width / CLAY_NCURSES_CELL_WIDTH);
                int h = (int)(box.height / CLAY_NCURSES_CELL_HEIGHT);

                // Apply Scissor
                int dx, dy, dw, dh;
                if (!Clay_Ncurses_IntersectScissor(x, y, w, h, &dx, &dy, &dw, &dh)) continue;

                // Color
                short fg = Clay_Ncurses_GetColorId(command->renderData.rectangle.backgroundColor);
                short bg = fg; // Solid block
                int pair = Clay_Ncurses_GetColorPair(fg, bg);

                attron(COLOR_PAIR(pair));
                for (int row = dy; row < dy + dh; row++) {
                    for (int col = dx; col < dx + dw; col++) {
                        mvaddch(row, col, ' ');
                    }
                }
                attroff(COLOR_PAIR(pair));
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                // Text is tricky with clipping. 
                // We need to clip the string and the position.
                int x = (int)(box.x / CLAY_NCURSES_CELL_WIDTH);
                int y = (int)(box.y / CLAY_NCURSES_CELL_HEIGHT);
                // Text width/height
                Clay_StringSlice text = command->renderData.text.stringContents;
                int w = text.length; 
                int h = 1;

                int dx, dy, dw, dh;
                if (!Clay_Ncurses_IntersectScissor(x, y, w, h, &dx, &dy, &dw, &dh)) continue;

                // Color (bg = -1 for transparent/default)
                short fg = Clay_Ncurses_GetColorId(command->renderData.text.textColor);
                int pair = Clay_Ncurses_GetColorPair(fg, -1);

                attron(COLOR_PAIR(pair));

                // Calculate substring to print based on clip
                // dx is the starting x on screen. x is original start.
                // offset in string = dx - x
                int offset = dx - x;
                int len = dw;

                if (offset >= 0 && offset < text.length) {
                    mvaddnstr(dy, dx, text.chars + offset, len);
                }

                attroff(COLOR_PAIR(pair));
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                int x = (int)(box.x / CLAY_NCURSES_CELL_WIDTH);
                int y = (int)(box.y / CLAY_NCURSES_CELL_HEIGHT);
                int w = (int)(box.width / CLAY_NCURSES_CELL_WIDTH);
                int h = (int)(box.height / CLAY_NCURSES_CELL_HEIGHT);

                // TODO: Robust border culling. For now, check if the whole rect intersects AT ALL
                int dx, dy, dw, dh;
                if (!Clay_Ncurses_IntersectScissor(x, y, w, h, &dx, &dy, &dw, &dh)) continue;

                short color = Clay_Ncurses_GetColorId(command->renderData.border.color);
                int pair = Clay_Ncurses_GetColorPair(color, -1);
                attron(COLOR_PAIR(pair));

                // Naive drawing (does not strictly respect scissor for PARTIAL borders, only fully skipped ones if outside)
                // Truly correct way handles each line.
                // Top
                if (y >= dy && y < dy + dh) {
                    int sx = x + 1, sw = w - 2;
                    // Intersect line with scissor X
                    int lx = (sx > dx) ? sx : dx;
                    int rx = (sx + sw < dx + dw) ? (sx + sw) : (dx + dw);
                    if (lx < rx) mvhline(y, lx, ACS_HLINE, rx - lx);
                }
                // Bottom
                if (y + h - 1 >= dy && y + h - 1 < dy + dh) {
                    int sx = x + 1, sw = w - 2;
                    int lx = (sx > dx) ? sx : dx;
                    int rx = (sx + sw < dx + dw) ? (sx + sw) : (dx + dw);
                    if (lx < rx) mvhline(y + h - 1, lx, ACS_HLINE, rx - lx);
                }
                // Left
                if (x >= dx && x < dx + dw) {
                    int sy = y + 1, sh = h - 2;
                    int ty = (sy > dy) ? sy : dy;
                    int by = (sy + sh < dy + dh) ? (sy + sh) : (dy + dh);
                    if (ty < by) mvvline(ty, x, ACS_VLINE, by - ty);
                }
                // Right
                if (x + w - 1 >= dx && x + w - 1 < dx + dw) {
                    int sy = y + 1, sh = h - 2;
                    int ty = (sy > dy) ? sy : dy;
                    int by = (sy + sh < dy + dh) ? (sy + sh) : (dy + dh);
                    if (ty < by) mvvline(ty, x + w - 1, ACS_VLINE, by - ty);
                }
                // Corners (simple visibility check)
                if (x >= dx && x < dx + dw && y >= dy && y < dy + dh) mvaddch(y, x, ACS_ULCORNER);
                if (x + w - 1 >= dx && x + w - 1 < dx + dw && y >= dy && y < dy + dh) mvaddch(y, x + w - 1, ACS_URCORNER);
                if (x >= dx && x < dx + dw && y + h - 1 >= dy && y + h - 1 < dy + dh) mvaddch(y + h - 1, x, ACS_LLCORNER);
                if (x + w - 1 >= dx && x + w - 1 < dx + dw && y + h - 1 >= dy && y + h - 1 < dy + dh) mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);

                attroff(COLOR_PAIR(pair));
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
                if (_scissorStackIndex < MAX_SCISSOR_STACK_DEPTH - 1) {
                    Clay_BoundingBox current = _scissorStack[_scissorStackIndex];
                    Clay_BoundingBox next = command->boundingBox;

                    // Intersect next with current
                    float nX = (next.x > current.x) ? next.x : current.x;
                    float nY = (next.y > current.y) ? next.y : current.y;
                    float nR = ((next.x + next.width) < (current.x + current.width)) ? (next.x + next.width) : (current.x + current.width);
                    float nB = ((next.y + next.height) < (current.y + current.height)) ? (next.y + next.height) : (current.y + current.height);

                    _scissorStackIndex++;
                    _scissorStack[_scissorStackIndex] = (Clay_BoundingBox){ nX, nY, nR - nX, nB - nY };
                }
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
                if (_scissorStackIndex > 0) {
                    _scissorStackIndex--;
                }
                break;
            }
            default: break;
        }
    }

    refresh();
}

// -------------------------------------------------------------------------------------------------
// -- Internal Helpers
// -------------------------------------------------------------------------------------------------

static bool Clay_Ncurses_IntersectScissor(int x, int y, int w, int h, int *outX, int *outY, int *outW, int *outH) {
    Clay_BoundingBox clip = _scissorStack[_scissorStackIndex];

    // Convert clip to cell coords
    int cx = (int)(clip.x / CLAY_NCURSES_CELL_WIDTH);
    int cy = (int)(clip.y / CLAY_NCURSES_CELL_HEIGHT);
    int cw = (int)(clip.width / CLAY_NCURSES_CELL_WIDTH);
    int ch = (int)(clip.height / CLAY_NCURSES_CELL_HEIGHT);

    // Intersect 
    int ix = (x > cx) ? x : cx;
    int iy = (y > cy) ? y : cy;
    int right = (x + w < cx + cw) ? (x + w) : (cx + cw);
    int bottom = (y + h < cy + ch) ? (y + h) : (cy + ch);

    int iw = right - ix;
    int ih = bottom - iy;

    if (iw <= 0 || ih <= 0) return false;

    *outX = ix;
    *outY = iy;
    *outW = iw;
    *outH = ih;
    return true;
}

static short Clay_Ncurses_GetColorId(Clay_Color color) {
    // 3-bit Color Mapping (Simple thresholding)
    int r = color.r > 128;
    int g = color.g > 128;
    int b = color.b > 128;

    if (r && g && b) return COLOR_WHITE;
    if (!r && !g && !b) return COLOR_BLACK;
    if (r && g) return COLOR_YELLOW;
    if (r && b) return COLOR_MAGENTA;
    if (g && b) return COLOR_CYAN;
    if (r) return COLOR_RED;
    if (g) return COLOR_GREEN;
    if (b) return COLOR_BLUE;

    return COLOR_WHITE;
}

static int Clay_Ncurses_GetColorPair(short fg, short bg) {
    // Check cache
    for (int i = 0; i < _colorPairCacheSize; i++) {
        if (_colorPairCache[i].fg == fg && _colorPairCache[i].bg == bg) {
            return _colorPairCache[i].pairId;
        }
    }

    // Create new
    if (_colorPairCacheSize >= MAX_COLOR_PAIRS_CACHE) {
        // Full? Just return last one or default.
        // Real impl: LRU eviction.
        return 0; // Default
    }

    int newId = _colorPairCacheSize + 1;
    init_pair(newId, fg, bg);

    _colorPairCache[_colorPairCacheSize].fg = fg;
    _colorPairCache[_colorPairCacheSize].bg = bg;
    _colorPairCache[_colorPairCacheSize].pairId = newId;
    _colorPairCacheSize++;

    return newId;
}
