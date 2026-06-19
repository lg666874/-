#ifndef MAINWIND_H
#define MAINWIND_H

#include <QMainWindow>
#include "ijkmediaplayer.h"
namespace Ui {
class MainWind;
}

class MainWind : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWind(QWidget *parent = 0);
    ~MainWind();

    int InitSignalsAndSlots();
    int message_loop(void *arg);
    int OutputVideo(const Frame *frame);
    void OnPlay();
    void OnStop();
    void OnPause();
    void OnStep();
    void screenshot_();
    void Add_volume_();
    void go_ahead_();
    void go_back_();
    void OnChange_volume();
    void OnChange_play();
    void OnChange_speed();
    void OnChange_muted();
    // SDL相关资源
//    SDL_Window *sdlWindow = nullptr;
//    SDL_Renderer *sdlRenderer = nullptr;
//    SDL_Texture *sdlTexture = nullptr;

    // 封装SDL渲染一帧的函数
    void renderOneFrame();


private:
    Ui::MainWind *ui;
    IjkMediaPlayer *mp_ = NULL;
    const char *play_name = NULL;
    char* play_name1 = NULL;
protected:
    // ✅ 核心声明：按键按下事件函数【必须写在protected下 + 带override】
    void keyPressEvent(QKeyEvent *event) override;
};

#endif // MAINWIND_H
