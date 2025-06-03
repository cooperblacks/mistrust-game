// 1) Install dependencies: sudo apt install build-essential libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev libsdl2-gfx-dev libzip-dev
// 2) Compilation: gcc -o game menu.c -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_gfx -lSDL2_mixer
// 3) Run: ./game

// menu.c

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define NUM_SQUARES 20

typedef struct {
    int x, y;
    int size;
    SDL_Color color;
} Square;

typedef struct {
    SDL_Rect rect;
    const char* label;
    bool hover;
    bool visible;
} Button;

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font *titleFont = NULL, *menuFont = NULL, *textFont = NULL;
SDL_Texture* mascotTexture = NULL;

Mix_Chunk* hoverSound = NULL;
Mix_Chunk* clickSound = NULL;
Mix_Music* themeMusic = NULL;

Square squares[NUM_SQUARES];
Button buttons[6];
char helpText[8192] = {0};

bool showMenu = false, showHelp = false, reverseMenu = false, showmascotImage = false;
int buttonX = -300, helpTextX = -400;
int mascotY = SCREEN_HEIGHT;
int introStep = 0;
float introAlpha = 0.0f;
const char* introTexts[] = {
    "Cooper Black presents...",
    "a game written in C",
    "Mistrust"
};
int titleY = 300;
bool subtitleVisible = false;

void drawText(const char* text, TTF_Font* font, SDL_Color color, int x, int y, float alpha) {
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_SetTextureAlphaMod(texture, (Uint8)(alpha * 255));
    SDL_Rect dst = { x, y, surface->w, surface->h };
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void drawIntro() {
    if (introStep >= 3) return;
    const char* msg = introTexts[introStep];
    SDL_Color white = {255, 255, 255, 255};
    int textW = 0, textH = 0;
    TTF_SizeUTF8(menuFont, msg, &textW, &textH);
    drawText(msg, introStep == 2 ? titleFont : menuFont, white,
             (SCREEN_WIDTH - textW) / 2, SCREEN_HEIGHT / 2 - textH / 2, introAlpha);

    if (introAlpha < 1.0f) {
        introAlpha += 0.01f;
    } else {
        introAlpha = 0.0f;
        introStep++;
        if (introStep >= 3) {
            showMenu = true;
            showmascotImage = true;
        }
    }
}

void drawSquares() {
    for (int i = 0; i < NUM_SQUARES; ++i) {
        squares[i].x -= 2;
        if (squares[i].x < -squares[i].size) {
            squares[i].x = SCREEN_WIDTH;
            squares[i].y = rand() % SCREEN_HEIGHT;
        }
        boxRGBA(renderer, squares[i].x, squares[i].y,
                squares[i].x + squares[i].size,
                squares[i].y + squares[i].size,
                squares[i].color.r,
                squares[i].color.g,
                squares[i].color.b,
                255);
    }
}

void drawButtons() {
    SDL_Color white = {255, 255, 255, 255};
    for (int i = 0; i < 6; ++i) {
        if (!buttons[i].visible) continue;
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &buttons[i].rect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &buttons[i].rect);
        drawText(buttons[i].label, menuFont,
                 buttons[i].hover ? (SDL_Color){255, 255, 0, 255} : white,
                 buttons[i].rect.x + 10, buttons[i].rect.y + 5, 1.0f);
    }
}

void updateButtonPositions(int x) {
    for (int i = 0; i < 5; ++i) {
        buttons[i].rect.x = x;
        buttons[i].visible = !reverseMenu;
    }
}

void drawHelpText() {
    SDL_Color white = {255, 255, 255, 255};
    int y = 200;
    
    // Use a temporary buffer to avoid modifying helpText
    char tempBuffer[sizeof(helpText)];
    strncpy(tempBuffer, helpText, sizeof(tempBuffer) - 1);
    tempBuffer[sizeof(tempBuffer) - 1] = '\0';

    char* line = strtok(tempBuffer, "\n");
    while (line) {
        drawText(line, textFont, white, helpTextX, y, 1.0f);
        y += 30; // Adjust spacing as needed
        line = strtok(NULL, "\n");
    }
}

void loadHelpContent() {
    FILE* file = fopen("Assets/help.txt", "r");
    if (file) {
        size_t bytesRead = fread(helpText, sizeof(char), sizeof(helpText) - 1, file);
        helpText[bytesRead] = '\0';  // null-terminate safely
        fclose(file);
    } else {
        snprintf(helpText, sizeof(helpText), "Failed to load help.txt");
    }
}

void playSound(Mix_Chunk* sound) {
    if (sound) Mix_PlayChannel(-1, sound, 0);
}

void initButtons() {
    const char* labels[] = {"Play", "Load Memory", "Settings", "Help", "Exit", "Back"};
    for (int i = 0; i < 6; ++i) {
        buttons[i].label = labels[i];
        buttons[i].hover = false;
        buttons[i].visible = (i < 5);
        buttons[i].rect = (SDL_Rect){buttonX, 360 + i * 60, 250, 40};
    }
    buttons[5].visible = false; // Back button
}

void drawmascot() {
    if (mascotTexture && showmascotImage) {
        int w, h;
        SDL_QueryTexture(mascotTexture, NULL, NULL, &w, &h);
        float scale = (float)SCREEN_HEIGHT / h;
        int newW = (int)(w * scale);
        SDL_Rect dst = {SCREEN_WIDTH - newW, mascotY, newW, SCREEN_HEIGHT};
        SDL_RenderCopy(renderer, mascotTexture, NULL, &dst);
        if (mascotY > 0) mascotY -= 5;
    }
}

bool directoryExists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

void extractAssetsIfNeeded() {
    if (!directoryExists("Assets")) {
        printf("Extracting assets.o...\n");
        system("unzip -o assets.o");  // requires unzip command-line tool
    }
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    TTF_Init();
    IMG_Init(IMG_INIT_PNG);
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    
    extractAssetsIfNeeded();

    window = SDL_CreateWindow("Mistrust (Game)",
              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
              SCREEN_WIDTH, SCREEN_HEIGHT,
              SDL_WINDOW_SHOWN);  // show window decorations

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    
    SDL_Surface* cursorSurface = IMG_Load("Assets/cursor.png");
if (cursorSurface) {
    SDL_Cursor* cursor = SDL_CreateColorCursor(cursorSurface, 0, 0); // hotspot at (0, 0)
    SDL_SetCursor(cursor);
    SDL_ShowCursor(SDL_ENABLE); // make sure cursor is visible
    SDL_FreeSurface(cursorSurface); // no longer needed
} else {
    printf("Failed to load cursor image: %s\n", IMG_GetError());
}

    titleFont = TTF_OpenFont("Assets/eternal.ttf", 50);
    menuFont = TTF_OpenFont("Assets/LeningradDisco.ttf", 24);
    textFont = TTF_OpenFont("Assets/Gugi.ttf", 20);

    mascotTexture = IMG_LoadTexture(renderer, "Assets/mascot.png");

    themeMusic = Mix_LoadMUS("Assets/theme.wav");
    hoverSound = Mix_LoadWAV("Assets/hover.wav");
    clickSound = Mix_LoadWAV("Assets/click.wav");

    if (themeMusic) Mix_PlayMusic(themeMusic, -1);

    initButtons();

    for (int i = 0; i < NUM_SQUARES; ++i) {
        squares[i].x = SCREEN_WIDTH + i * 64;
        squares[i].y = rand() % SCREEN_HEIGHT;
        squares[i].size = 64;
        squares[i].color = (SDL_Color){0,0,rand() % 256, 255};
    }

    bool running = true;
    SDL_Event e;
    Uint32 last = SDL_GetTicks();

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;
            if (e.type == SDL_MOUSEMOTION) {
                for (int i = 0; i < 6; ++i) {
                    if (!buttons[i].visible) continue;
                    SDL_Point p = {e.motion.x, e.motion.y};
                    bool hovered = SDL_PointInRect(&p, &buttons[i].rect);
                    if (hovered && !buttons[i].hover) playSound(hoverSound);
                    buttons[i].hover = hovered;
                }
            }
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                for (int i = 0; i < 6; ++i) {
                    if (buttons[i].hover && buttons[i].visible) {
                        playSound(clickSound);
                        if (i == 3) {  // Help
                            reverseMenu = true;
                            loadHelpContent();
                            showHelp = true;
                            buttons[5].visible = true;
                        } else if (i == 4) {  // Exit
                            running = false;
                        } else if (i == 5) {  // Back
                            showHelp = false;
                            reverseMenu = false;
                            buttonX = -300;
                            helpTextX = -400;
                            updateButtonPositions(buttonX);
                            buttons[5].visible = false;
                            loadHelpContent(); 
                        }
                    }
                }
            }
        }

       SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Clear screen to black (the true background)
       SDL_RenderClear(renderer);

        if (showMenu) drawSquares();  // Only draw background after intro

        if (introStep < 3) {
            drawIntro();
        } else {
            if (titleY > 20) titleY -= 2;
            drawText("Mistrust", titleFont,
                     (SDL_Color){255, 255, 255, 255}, 50, titleY, 1.0f);
            if (titleY <= 20) {
                subtitleVisible = true;
                drawText("version 0.1 2025-06-02 early alpha", textFont,
                         (SDL_Color){255, 255, 255, 255}, 50, 80, 1.0f);
            }

            if (buttonX < 50 && !reverseMenu) {
                buttonX += 2;
                updateButtonPositions(buttonX);
            } else if (reverseMenu && buttonX > -300) {
                buttonX -= 5;
                updateButtonPositions(buttonX);
            }

            drawButtons();

            if (showHelp && helpTextX < 50) helpTextX += 5;
            if (showHelp) {
                drawHelpText();
                buttons[5].rect.x = 100;
                buttons[5].rect.y = 650;
            }
        }

        drawmascot();
        SDL_RenderPresent(renderer);

        Uint32 now = SDL_GetTicks();
        Uint32 delta = now - last;
        if (delta < 30) SDL_Delay(30 - delta);
        last = now;
    }

    Mix_Quit();
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    return 0;
}

