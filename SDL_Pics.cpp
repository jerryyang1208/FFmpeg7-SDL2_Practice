#include <iostream>
extern "C" {
    #include <SDL.h>
}
#define POINTS_COUNT 4

// 四个点构成的线条（白色三角形）
SDL_Point points[POINTS_COUNT] = {
    {320, 200},
    {300, 240},
    {340, 240},
    {320, 200}
};

// 蓝色大矩形
SDL_Rect bigRect = {50, 50, 300, 200};

// 青色小矩形
SDL_Rect smallRect = {400, 300, 100, 100};

// 初始化 SDL 窗口和渲染器
bool initSDL(SDL_Window** window, SDL_Renderer** renderer) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cout << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }
    
    *window = SDL_CreateWindow("SDL2 Example",  // 通过二级指针修改调用者的window指针
                               SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                               640, 480, SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);
    if (*window == nullptr) {
        std::cout << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
    if (*renderer == nullptr) {
        std::cout << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    return true;
}

// 渲染所有图形
void render(SDL_Renderer* renderer) {
    // 背景颜色: 红色
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderClear(renderer);

    // 绘制蓝色大矩形
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    SDL_RenderFillRect(renderer, &bigRect);

    // 绘制白色三角形
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawLines(renderer, points, POINTS_COUNT);

    // 绘制青色小矩形
    SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
    SDL_RenderFillRect(renderer, &smallRect);

    SDL_RenderPresent(renderer);  // 显示图像
}

int main(int argc, char* argv[]) {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    if (!initSDL(&window, &renderer)) { // 传入二级指针，initSDL 内部修改指针指向
        return 1; // 初始化失败
    }

    render(renderer);

    SDL_Delay(5000);  // 显示 5 秒

    // 清理资源
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}