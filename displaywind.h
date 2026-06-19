#ifndef DISPLAYWIND_H
#define DISPLAYWIND_H

#include <QWidget>
#include <QMutex>
#include <QMutexLocker>
#include "ijkmediaplayer.h"
#include "mainwind.h"

#include <QPaintEvent>
#include <QKeyEvent>
#include <QFile>
#include <QTimer>

// 关键：改为 OpenGL 窗口基类
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>

#define YUV420P 0
#define NV12    1

struct FrameData{
    uint8_t *data;
    int width;
    int height;
};

struct YUVChannelData {
    GLuint texY = 0;
    GLuint texU = 0;
    GLuint texV = 0;
    int width = 0;
    int height = 0;
    uchar* dataY = nullptr;
    uchar* dataU = nullptr;
    uchar* dataV = nullptr;
    int lineSizeY = 0;
    int lineSizeU = 0;
    int lineSizeV = 0;
    bool yuyv = true;
    bool valid = false;
};

namespace Ui {
class DisplayWind;
}

class ImageScaler
{
public:
    ImageScaler(void){}
};

// ===================== 核心修改 =====================
// 从 QWidget 改为 QOpenGLWidget + QOpenGLFunctions
// ====================================================
class DisplayWind : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit DisplayWind(QWidget *parent = 0);
    ~DisplayWind();

    // 你原有视频绘制接口（保留）
    int Draw(const Frame * frame);

    // 构造时传入MainWind对象（保留）
    DisplayWind(MainWind* mainWind) : m_mainWind(mainWind) {}

protected:
    // ===================== 核心修改 =====================
    // 删掉 QWidget 的 paintEvent，改用 OpenGL 三个函数
    // ====================================================
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

    // 你原来的 paintEvent 可以删掉，OpenGL 不再需要

private:
    // ===================== 你原有全部成员（完整保留） =====================
    Ui::DisplayWind *ui;
    int m_nLastFrameWidth;
    int m_nLastFrameHeigth;
    uint8_t *data;
    int win_height;
    int win_width;
    bool is_dispaly_size_change_ = false;
    int x_ = 0;
    int y_ = 0;
    int video_width = 0;
    int video_heigth = 0;
    int img_width = 0;
    int img_height = 0;
    uint8_t *y,*u,*v;
    QImage img;
    QImage target_image,scale_image;
    int yuv420p_to_rgb888(AVFrame *yuv_frame,int width,int height,uint8_t* dst_data);
    QMutex m_mutex;
    ImageScaler *ImageScaler_ = NULL;
    MainWind* m_mainWind;

    // ===================== 移植进来的 OpenGL 渲染成员（完整可用） =====================
    bool yuyv = false;
    int width_ = 1152, height_ = 722;
    int wid_count_ = 2, hgt_count_ = 2;

    quint8 *dataY = nullptr;
    quint8 *dataU = nullptr, *dataV = nullptr;
    quint8 *dataUV = nullptr;

    quint32 lineSizeY, lineSizeU, lineSizeV, lineSizeUV;
    QString shaderVert, shaderFrag;

    QOpenGLShaderProgram program;
    GLuint textureY = 0, textureU = 0, textureV = 0, textureUV = 0;
    GLuint textureUniformY = 0, textureUniformU = 0, textureUniformV = 0;

    int format_ = YUV420P;
    GLint m_isDrawFirstUniform = 0;
    GLint m_drawChannelUniform = 0;
    QOpenGLBuffer vbo;

    GLuint VBO = 0, VAO = 0;
    GLint m_mvpUniform = 0;
    QVector<YUVChannelData> m_chanData;

    // OpenGL 内部方法
    void initData();
    void initColor();
    void initShader();
    void initTextures();
    void initParamete();
    void deleteTextures();
    void yuv420pPaintGL();
    void nv12PaintGL();

    // 公开给外部调用的纹理更新接口（你 Draw 函数里调用）
public:
    void setYuyv(bool yuyv);
    void clear();
    void setFrameSize(int width, int height);
    void updateTextures(quint8 *dataY, quint8 *dataU, quint8 *dataV, quint32 linesizeY, quint32 linesizeU, quint32 linesizeV);
    void updateTextures(quint8 *dataY, quint8 *dataUV, quint32 linesizeY, quint32 linesizeUV);
    void updateFrame(int width, int height, quint8 *dataY, quint8 *dataU, quint8 *dataV, quint32 linesizeY, quint32 linesizeU, quint32 linesizeV);
    void updateFrame(int width, int height, quint8 *dataY, quint8 *dataUV, quint32 linesizeY, quint32 linesizeUV);
};

#endif // DISPLAYWIND_H
