#define CLAY_IMPLEMENTATION
#include "../../clay.h"
#include "../../renderers/ncurses/clay_renderer_ncurses.c"
#include <unistd.h> // for usleep

#define DEFAULT_SCROLL_DELTA 3.0f

// State for the example
typedef struct {
    bool sidebarOpen;
    float scrollDelta;
    bool shouldQuit;
} AppState;

static AppState appState = { .sidebarOpen = true, .scrollDelta = 0.0f, .shouldQuit = false };

void HandleInput() {
    // Reset delta per frame
    appState.scrollDelta = 0.0f;

    int ch;
    while ((ch = getch()) != ERR) {
        if (ch == 'q' || ch == 'Q') {
            appState.shouldQuit = true;
        }
        if (ch == 's' || ch == 'S') {
            appState.sidebarOpen = !appState.sidebarOpen;
        }
        if (ch == KEY_UP) {
            appState.scrollDelta += DEFAULT_SCROLL_DELTA;
        }
        if (ch == KEY_DOWN) {
            appState.scrollDelta -= DEFAULT_SCROLL_DELTA;
        }
    }
}

void RenderSidebar() {
    if (!appState.sidebarOpen) return;

    CLAY(CLAY_ID("Sidebar"), {
        .layout = { 
            .sizing = { CLAY_SIZING_FIXED(240), CLAY_SIZING_GROW() }, // 30 cells wide
            .padding = CLAY_PADDING_ALL(16),
            .childGap = 16,
            .layoutDirection = CLAY_TOP_TO_BOTTOM
        },
        .backgroundColor = {30, 30, 30, 255}, // Dark Gray
        .border = { .color = {255, 255, 255, 255}, .width = { .right = 2 } } // White Border Right
    }) {
        CLAY_TEXT(CLAY_STRING("SIDEBAR"), CLAY_TEXT_CONFIG({ 
            .textColor = {255, 255, 0, 255} 
        }));

        CLAY(CLAY_ID("SidebarItem1"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(32) } },
            .backgroundColor = {60, 60, 60, 255}
        }) {
            CLAY_TEXT(CLAY_STRING(" > Item 1"), CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255} }));
        }

        CLAY(CLAY_ID("SidebarItem2"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(32) } },
            .backgroundColor = {60, 60, 60, 255}
        }) {
            CLAY_TEXT(CLAY_STRING(" > Item 2"), CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255} }));
        }
    }
}

// Helpers for "Realistic" Data
const char* NAMES[] = {
    "Alice",
    "Bob",
    "Charlie",
    "Diana",
    "Ethan",
    "Fiona",
    "George",
    "Hannah"
};
const char* TITLES[] = {
    "Just released a new library!",
    "Thoughts on C programming?",
    "Check out this cool algorithm",
    "Why I love Ncurses",
    "Clay UI is pretty flexible",
    "Debugging segfaults all day...",
    "Coffee break time ☕",
    "Anyone going to the conf?"
};
const char* LOREM[] = {
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit.",
    "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.",
    "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris.",
    "Duis aute irure dolor in reprehenderit in voluptate velit esse cillum.",
    "Excepteur sint occaecat cupidatat non proident, sunt in culpa."
};

void RenderPost(int index) {
    CLAY(CLAY_IDI("Post", index), {
        .layout = { 
            .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT(0) },
            .padding = CLAY_PADDING_ALL(16),
            .childGap = 8,
            .layoutDirection = CLAY_TOP_TO_BOTTOM
        },
        .backgroundColor = {25, 25, 25, 255},
        .cornerRadius = {8}, // Rounded corners (will render as square in TUI usually unless ACS handled)
        .border = { .color = {60, 60, 60, 255}, .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 } }
    }) {
        // Post Header: Avatar + Name + Time
        CLAY(CLAY_IDI("PostHeader", index), {
            .layout = { 
                .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT(0) },
                .childGap = 12,
                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                .layoutDirection = CLAY_LEFT_TO_RIGHT
            }
        }) {
            // Avatar
            CLAY(CLAY_IDI("Avatar", index), {
                .layout = { .sizing = { CLAY_SIZING_FIXED(32), CLAY_SIZING_FIXED(16) } }, // 2x1 cells approx
                .backgroundColor = { (index * 50) % 255, (index * 80) % 255, (index * 30) % 255, 255 },
                .cornerRadius = {8}
            }) {}

            // Name & Title
            CLAY(CLAY_IDI("AuthorInfo", index), {
                .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT(0) }, .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 4 }
            }) {
                Clay_String name = { .length = strlen(NAMES[index % 8]), .chars = NAMES[index % 8] };
                Clay_String title = { .length = strlen(TITLES[index % 8]), .chars = TITLES[index % 8] };
                CLAY_TEXT(name, CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255} }));
                CLAY_TEXT(title, CLAY_TEXT_CONFIG({ .textColor = {150, 150, 150, 255} }));
            }
        }

        // Post Body
        CLAY(CLAY_IDI("PostBody", index), {
            .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT(0) }, .padding = { .top = 8, .bottom = 8 } }
        }) {
            Clay_String lorem = { .length = strlen(LOREM[index % 5]), .chars = LOREM[index % 5] };
            CLAY_TEXT(lorem, CLAY_TEXT_CONFIG({ .textColor = {200, 200, 200, 255} }));
        }

        // Post Actions
        CLAY(CLAY_IDI("PostActions", index), {
            .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT(0) }, .childGap = 16, .layoutDirection = CLAY_LEFT_TO_RIGHT }
        }) {
            CLAY_TEXT(CLAY_STRING("[ Like ]"), CLAY_TEXT_CONFIG({ .textColor = {100, 200, 100, 255} }));
            CLAY_TEXT(CLAY_STRING("[ Comment ]"), CLAY_TEXT_CONFIG({ .textColor = {100, 150, 255, 255} }));
            CLAY_TEXT(CLAY_STRING("[ Share ]"), CLAY_TEXT_CONFIG({ .textColor = {200, 100, 100, 255} }));
        }
    }
}

void RenderContent() {
    CLAY(CLAY_ID("ContentArea"), {
        .layout = { 
            .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
            .padding = CLAY_PADDING_ALL(16),
            .childGap = 16,
            .layoutDirection = CLAY_TOP_TO_BOTTOM
        },
        .backgroundColor = {10, 10, 10, 255}
    }) {
        // Sticky Header
        CLAY(CLAY_ID("Header"), {
            .layout = { 
                .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(48) }, // 3 cells high
                .padding = { .left = 16, .right=16 },
                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER }
            },
            .backgroundColor = {0, 0, 80, 255},
            .border = { .color = {0, 100, 255, 255}, .width = { .bottom = 1 } }
        }) {
            CLAY_TEXT(CLAY_STRING("Clay Social Feed"), CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255} }));
        }

        // Scroll Viewport
        // We use SIZING_GROW for height so it fills the remaining screen space.
        CLAY(CLAY_ID("Viewport"), {
            .layout = { 
                .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() }, 
                .padding = { .top = 8, .bottom = 8 }
            },
            .clip = { .vertical = true, .childOffset = Clay_GetScrollOffset() },
            .backgroundColor = {15, 15, 15, 255}
        }) {
            CLAY(CLAY_ID("FeedList"), {
                .layout = { 
                    .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT(0) }, // Fit height to content (allows it to be taller than viewport)
                    .childGap = 16, 
                    .layoutDirection = CLAY_TOP_TO_BOTTOM 
                }
            }) {
                // Get first item pos if possible
                Clay_ElementData item0 = Clay_GetElementData(CLAY_IDI("Post", 0));

                for (int i = 0; i < 50; ++i) { // 50 Posts
                    RenderPost(i);
                }

                CLAY_TEXT(CLAY_STRING("--- End of Feed ---"), CLAY_TEXT_CONFIG({ .textColor = {140, 140, 140, 255} }));
            }
        }

        CLAY_TEXT(CLAY_STRING("Controls: ARROW UP/DOWN to Scroll | Q to Quit | S to Toggle Sidebar"), CLAY_TEXT_CONFIG({ .textColor = {120, 120, 120, 255} }));
    }
}

void RenderMainLayout() {
    CLAY(CLAY_ID("Root"), {
        .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() }, .layoutDirection = CLAY_LEFT_TO_RIGHT },
        .backgroundColor = {0, 0, 0, 255}
    }) {
        RenderSidebar();
        RenderContent();
    }
}

int main() {
    uint32_t minMemory = Clay_MinMemorySize();
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(minMemory, malloc(minMemory));

    Clay_Initialize(arena, (Clay_Dimensions){0,0}, (Clay_ErrorHandler){NULL});
    Clay_SetMeasureTextFunction(Clay_Ncurses_MeasureText, NULL);
    Clay_Ncurses_Initialize();

    // Non-blocking input
    timeout(0); 

    while(!appState.shouldQuit) {
        HandleInput();

        Clay_Dimensions dims = Clay_Ncurses_GetLayoutDimensions();
        Clay_SetLayoutDimensions(dims);

        // Handle Scroll Logic
        Clay_ElementId viewportId = CLAY_ID("Viewport");
        Clay_ElementData viewportData = Clay_GetElementData(viewportId);

        if (viewportData.found) {
            Clay_Vector2 center = { 
                viewportData.boundingBox.x + viewportData.boundingBox.width / 2,
                viewportData.boundingBox.y + viewportData.boundingBox.height / 2
            };
            Clay_SetPointerState(center, false);

            Clay_UpdateScrollContainers(true, (Clay_Vector2){0, appState.scrollDelta}, 0.016f);
        }

        Clay_BeginLayout();
        RenderMainLayout();
        Clay_RenderCommandArray commands = Clay_EndLayout();

        Clay_Ncurses_Render(commands);

        usleep(32000); 
    }

    Clay_Ncurses_Terminate();
    free(arena.memory);
    return 0;
}
