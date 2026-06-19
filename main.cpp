#include "mainwind.h"
#include "displaywind.h"
#include <QApplication>
#include <QDebug>

#define SDL_INIT_AUDIO          0x00000010
#define SDL_INIT_VIDEO          0x00000020  /**< SDL_INIT_VIDEO implies SDL_INIT_EVENTS */
#undef main
SDL_Window *sdlWindow = nullptr;
SDL_Renderer *sdlRenderer = nullptr;
SDL_Texture *sdlTexture = nullptr;
extern int sdl_width;
extern int sdl_height;
#define YUV_WIDTH sdl_width
#define YUV_HEIGHT sdl_height

int main(int argc, char *argv[])
{
    //注册ffmpeg中相关
    av_register_all();
    avformat_network_init();
    QApplication a(argc, argv);
    MainWind w;
    w.show();
    // for(int i = 0;i< 2; i++)
    // {

    // }
    // 通过对象指针调用成员函数
//            m_mainWind->renderOneFrame();

            // 模拟YUV420P数据（灰色帧)
//    QMutexLocker locker(&m_mutex);
    // 初始化SDL（视频子系统）
//     if (SDL_Init(SDL_INIT_VIDEO) < 0) {
//          qDebug() << "SDL_Init failed!：" << SDL_GetError();
//            return -1;
//        }
//     // 创建SDL窗口（可绑定到Qt窗口的句柄，实现嵌入Qt窗口，否则是独立窗口）
//         // 若要嵌入Qt窗口：SDL_CreateWindowFrom((void*)this->winId())
//      sdlWindow = SDL_CreateWindow("Dragon播放器",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,YUV_WIDTH,YUV_HEIGHT,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);

// //        // 设置渲染缩放质量：linear = 线性插值（缩放更平滑）
//          SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");//线性插值，缩放更平滑
// //        if (sdlWindow) {//创建完窗口
// //            sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);// 创建渲染器：优先硬件加速+垂直同步（减少画面撕裂）
// //            if (!sdlRenderer) {//硬件渲染失败，降级为软件渲染
// //                av_log(NULL, AV_LOG_WARNING, "Failed to initialize a hardware accelerated renderer: %s\n", SDL_GetError());
//     sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, 0);//降级为软件渲染
// //          //  }
//             //降级为软件渲染后继续获取渲染器
// //            if (sdlRenderer) { //获取渲染器并打印
// //                if (!SDL_GetRendererInfo(renderer, &renderer_info))//获取渲染器
// //                    av_log(NULL, AV_LOG_VERBOSE, "Initialized %s renderer.\n", renderer_info.name);
// //            }
// //        }

// //        sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED);
//   sdlTexture = SDL_CreateTexture(sdlRenderer,SDL_PIXELFORMAT_IYUV,SDL_TEXTUREACCESS_STREAMING,YUV_WIDTH,YUV_HEIGHT);
        // 窗口创建后，渲染一帧
//          renderOneFrame();


    return a.exec();
}
