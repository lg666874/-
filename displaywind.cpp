#include "displaywind.h"
#include "ui_displaywind.h"
#include "ffmsg_queue.h"
#include "mainwind.h"
#include <QPainter>
#include <QDebug>
#include <QPalette>

#define VersionString "#version 330\n"
//初始化片段着色器
inline void initFragment(QStringList &list) {
    bool useOpenGLES = false;
#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    useOpenGLES = QCoreApplication::testAttribute(Qt::AA_UseOpenGLES);
#endif
#ifdef __arm__
    useOpenGLES = true;
#endif
#ifdef Q_OS_ANDROID
    useOpenGLES = true;
#endif
    if (useOpenGLES) {
        list << "precision mediump int;";
        list << "precision mediump float;";
    }
}

DisplayWind::DisplayWind(QWidget *parent)
    : QOpenGLWidget(parent),
    ui(new Ui::DisplayWind)
{
    ui->setupUi(this);

    // 你原有成员初始化
    data = nullptr;
    m_nLastFrameWidth = 0;
    m_nLastFrameHeigth = 0;
    win_width = 0;
    win_height = 0;
    is_dispaly_size_change_ = false;
    x_ = 0;
    y_ = 0;
    video_width = 0;
    video_heigth = 0;
    img_width = 0;
    img_height = 0;
    ImageScaler_ = nullptr;
    m_mainWind = nullptr;
    y = new uint8_t[1920 * 1080];
    u = new uint8_t[1920 * 1080];
    v = new uint8_t[1920 * 1080];

}

DisplayWind::~DisplayWind()
{
    makeCurrent();
    if (format_ == NV12) {
        vbo.destroy();
    }
    doneCurrent();

    if (data) {
        free(data);
        data = nullptr;
    }
    if(y)delete y;
    if(u)delete u;
    if(v)delete v;

    delete ui;
}

// ==============================================
// 以下完全是你 Widget.cpp 移植的 OpenGL 渲染
// 100% 匹配头文件成员
// ==============================================

void DisplayWind::initializeGL()
{
    initializeOpenGLFunctions();
    glDisable(GL_DEPTH_TEST);

    // YUV420P 顶点/纹理坐标
    if (format_ == YUV420P) {

        // //多路渲染
        //
        // static const GLfloat tex[] = {
        //     0.0f, 1.0f*5,
        //     1.0f*5, 1.0f*5,
        //     0.0f, 0.0f,
        //     1.0f*5, 0.0f
        // };
        //

        //单路渲染

            static const GLfloat tex[] = {
                0.0f, 1.0f,
                1.0f, 1.0f,
                0.0f, 0.0f,
                1.0f, 0.0f
            };


        static const GLfloat ver_more[9][8] = {
            {-1.0f,  1.0f/3.0f, -1.0f/3.0f,  1.0f/3.0f, -1.0f,  1.0f, -1.0f/3.0f,  1.0f},
            {-1.0f/3.0f,  1.0f/3.0f,  1.0f/3.0f,  1.0f/3.0f, -1.0f/3.0f,  1.0f,  1.0f/3.0f,  1.0f},
            { 1.0f/3.0f,  1.0f/3.0f,  1.0f,  1.0f/3.0f,  1.0f/3.0f,  1.0f,  1.0f,  1.0f},
            {-1.0f, -1.0f/3.0f, -1.0f/3.0f, -1.0f/3.0f, -1.0f,  1.0f/3.0f, -1.0f/3.0f,  1.0f/3.0f},
            {-1.0f/3.0f, -1.0f/3.0f,  1.0f/3.0f, -1.0f/3.0f, -1.0f/3.0f,  1.0f/3.0f,  1.0f/3.0f,  1.0f/3.0f},
            { 1.0f/3.0f, -1.0f/3.0f,  1.0f, -1.0f/3.0f,  1.0f/3.0f,  1.0f/3.0f,  1.0f,  1.0f/3.0f},
            {-1.0f, -1.0f, -1.0f/3.0f, -1.0f, -1.0f, -1.0f/3.0f, -1.0f/3.0f, -1.0f/3.0f},
            {-1.0f/3.0f, -1.0f,  1.0f/3.0f, -1.0f, -1.0f/3.0f, -1.0f/3.0f,  1.0f/3.0f, -1.0f/3.0f},
            { 1.0f/3.0f, -1.0f,  1.0f, -1.0f,  1.0f/3.0f, -1.0f/3.0f,  1.0f, -1.0f/3.0f}
        };
        static const GLfloat ver1[] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};

        // //多路渲染
        // {
        // for (int i = 0; i < 9; ++i) {
        //     glVertexAttribPointer(i + 1, 2, GL_FLOAT, 0, 0, ver_more[i]);
        //     glEnableVertexAttribArray(i + 1);
        // }
        // }

        //单路全屏渲染
        {
        glVertexAttribPointer(1, 2, GL_FLOAT, 0, 0, ver1);
        glEnableVertexAttribArray(1);
        }

        glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, tex);
        glEnableVertexAttribArray(0);
    }

    initShader();
    initTextures();
    initColor();

    m_drawChannelUniform = program.uniformLocation("drawChannel");
}

void DisplayWind::paintGL()
{
    if (!dataY || width_ <= 0 || height_ <= 0) {
        initColor();
        return;
    }

    if (format_ == YUV420P) {
        yuv420pPaintGL();
    } else if (format_ == NV12) {
        nv12PaintGL();
    }
}

void DisplayWind::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void DisplayWind::yuv420pPaintGL()
{
    for (int ch = 0; ch < 9; ch++) {
        glUniform1i(m_drawChannelUniform, ch);

        // Y
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureY);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, lineSizeY);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width_, height_, 0,
                     GL_LUMINANCE, GL_UNSIGNED_BYTE, dataY);
        glUniform1i(textureUniformY, 0);
        //设置x轴模式
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,GL_REPEAT);//GL_CLAMP模式为边缘颜色颜色填充
        // 设置y轴模式
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);


        // U
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textureU);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, lineSizeU);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width_ >> 1, height_ >> 1, 0,
                     GL_LUMINANCE, GL_UNSIGNED_BYTE, dataU);
        glUniform1i(textureUniformU, 1);
        //设置x轴模式
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,GL_REPEAT);//GL_CLAMP模式为边缘颜色颜色填充
        // 设置y轴模式
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);


        // V
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, textureV);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, lineSizeV);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width_ >> 1, height_ >> 1, 0,
                     GL_LUMINANCE, GL_UNSIGNED_BYTE, dataV);
        glUniform1i(textureUniformV, 2);
        // 设置 S 轴（对应 X 轴）：重复
        //设置x轴模式
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,GL_REPEAT);//GL_CLAMP模式为边缘颜色颜色填充
        // 设置y轴模式
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
}

void DisplayWind::nv12PaintGL()
{
    program.bind();
    program.enableAttributeArray("vertexIn");
    program.enableAttributeArray("textureIn");
    program.setAttributeBuffer("vertexIn", GL_FLOAT, 0, 2, 2 * sizeof(float));
    program.setAttributeBuffer("textureIn", GL_FLOAT, 8 * sizeof(float), 2, 2 * sizeof(float));

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textureY);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, lineSizeY);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width_, height_, 0,
                 GL_RED, GL_UNSIGNED_BYTE, dataY);
    initParamete();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureUV);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, lineSizeUV >> 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, width_ >> 1, height_ >> 1, 0,
                 GL_RG, GL_UNSIGNED_BYTE, dataUV);
    initParamete();

    glDrawArrays(GL_QUADS, 0, 4);
    program.setUniformValue("textureY", 1);
    program.setUniformValue("textureUV", 0);
    program.disableAttributeArray("vertexIn");
    program.disableAttributeArray("textureIn");
    program.release();
}

void DisplayWind::initData()
{
    width_ = height_ = 0;
    dataY = dataU = dataV = dataUV = nullptr;
    lineSizeY = lineSizeU = lineSizeV = lineSizeUV = 0;
}

void DisplayWind::initColor()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void DisplayWind::initShader()
{
    QStringList list;

    if (format_ == YUV420P) {
        list << "attribute vec2 textureIn;";
        list << "attribute vec4 vertexIn;";
        list << "attribute vec4 vertexIn1;";
        list << "attribute vec4 vertexIn2;";
        list << "attribute vec4 vertexIn3;";
        list << "attribute vec4 vertexIn4;";
        list << "attribute vec4 vertexIn5;";
        list << "attribute vec4 vertexIn6;";
        list << "attribute vec4 vertexIn7;";
        list << "attribute vec4 vertexIn8;";
        list << "varying vec2 textureOut;";
        list << "uniform int drawChannel;";

        list << "void main(void){";
        list << "textureOut = textureIn;";
        list << "if (drawChannel == 0) gl_Position = vertexIn;";
        list << "else if (drawChannel == 1) gl_Position = vertexIn1;";
        list << "else if (drawChannel == 2) gl_Position = vertexIn2;";
        list << "else if (drawChannel == 3) gl_Position = vertexIn3;";
        list << "else if (drawChannel == 4) gl_Position = vertexIn4;";
        list << "else if (drawChannel == 5) gl_Position = vertexIn5;";
        list << "else if (drawChannel == 6) gl_Position = vertexIn6;";
        list << "else if (drawChannel == 7) gl_Position = vertexIn7;";
        list << "else if (drawChannel == 8) gl_Position = vertexIn8;";
        list << "}";

        shaderVert = list.join("");
        list.clear();

        initFragment(list);
        list << "varying mediump vec2 textureOut;";
        list << "uniform sampler2D textureY;";
        list << "uniform sampler2D textureU;";
        list << "uniform sampler2D textureV;";
        list << "void main(void){";
        list << "vec3 yuv; vec3 rgb;";
        list << "yuv.r = texture2D(textureY, textureOut).r;";
        list << "yuv.g = texture2D(textureU, textureOut).r - 0.5;";
        list << "yuv.b = texture2D(textureV, textureOut).r - 0.5;";
        list << "rgb = mat3(1.0, 1.0, 1.0, 0.0, -0.138, 1.816, 1.540, -0.459, 0.0) * yuv;";
        list << "gl_FragColor = vec4(rgb, 1.0);";
        list << "}";
        shaderFrag = list.join("");
    }

    program.addShaderFromSourceCode(QOpenGLShader::Vertex, shaderVert);
    program.addShaderFromSourceCode(QOpenGLShader::Fragment, shaderFrag);

    if (format_ == YUV420P) {
        program.bindAttributeLocation("textureIn", 0);
        program.bindAttributeLocation("vertexIn", 1);
        program.bindAttributeLocation("vertexIn1", 2);
        program.bindAttributeLocation("vertexIn2", 3);
        program.bindAttributeLocation("vertexIn3", 4);
        program.bindAttributeLocation("vertexIn4", 5);
        program.bindAttributeLocation("vertexIn5", 6);
        program.bindAttributeLocation("vertexIn6", 7);
        program.bindAttributeLocation("vertexIn7", 8);
        program.bindAttributeLocation("vertexIn8", 9);
    }

    program.link();
    program.bind();

    textureUniformY = program.uniformLocation("textureY");
    textureUniformU = program.uniformLocation("textureU");
    textureUniformV = program.uniformLocation("textureV");
}

void DisplayWind::initTextures()
{
    if (format_ == YUV420P) {
        glGenTextures(1, &textureY);
        glBindTexture(GL_TEXTURE_2D, textureY);
        initParamete();

        glGenTextures(1, &textureU);
        glBindTexture(GL_TEXTURE_2D, textureU);
        initParamete();

        glGenTextures(1, &textureV);
        glBindTexture(GL_TEXTURE_2D, textureV);
        initParamete();
    }
}

void DisplayWind::initParamete()
{
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
}

// ==============================================
// 公开接口（头文件已声明）
// ==============================================
void DisplayWind::setYuyv(bool yuyv) {
    this->yuyv = yuyv;
}

void DisplayWind::clear() {
    initData();
    update();
}

void DisplayWind::setFrameSize(int width, int height) {
    width_ = width;
    height_ = height;
}

void DisplayWind::updateTextures(quint8 *dataY, quint8 *dataU, quint8 *dataV,
                                 quint32 linesizeY, quint32 linesizeU, quint32 linesizeV)
{
    this->dataY = dataY;
    this->dataU = dataU;
    this->dataV = dataV;
    lineSizeY = linesizeY;
    lineSizeU = linesizeU;
    lineSizeV = linesizeV;
    update();
}

void DisplayWind::updateTextures(quint8 *dataY, quint8 *dataUV,
                                 quint32 linesizeY, quint32 linesizeUV)
{
    this->dataY = dataY;
    this->dataUV = dataUV;
    lineSizeY = linesizeY;
    lineSizeUV = linesizeUV;
    update();
}

void DisplayWind::updateFrame(int width, int height, quint8 *dataY, quint8 *dataU, quint8 *dataV,
                              quint32 linesizeY, quint32 linesizeU, quint32 linesizeV)
{
    setFrameSize(width, height);
    updateTextures(dataY, dataU, dataV, linesizeY, linesizeU, linesizeV);
}

void DisplayWind::updateFrame(int width, int height, quint8 *dataY, quint8 *dataUV,
                              quint32 linesizeY, quint32 linesizeUV)
{
    setFrameSize(width, height);
    updateTextures(dataY, dataUV, linesizeY, linesizeUV);
}

// ==============================================
// 你原来的 Draw 函数 → 直接喂 GPU
// ==============================================
int DisplayWind::Draw(const Frame *frame)
{

    if (!frame || !frame->frame)
        return 0;

    QMutexLocker lock(&m_mutex);


    // uint8_t *y = frame->frame->data[0];
    memcpy(y,frame->frame->data[0],sizeof(uint8_t)*frame->frame->width *frame->frame->height);
    // uint8_t *u = frame->frame->data[1];
    memcpy(u,frame->frame->data[1],sizeof(uint8_t)*frame->frame->width *frame->frame->height*1/4);
    // uint8_t *v = frame->frame->data[2];
    memcpy(v,frame->frame->data[2],sizeof(uint8_t)*frame->frame->width *frame->frame->height*1/4);

    int w = frame->width;
    int h = frame->height;

    updateFrame(w, h, y, u, v,
                frame->frame->linesize[0],
                frame->frame->linesize[1],
                frame->frame->linesize[2]);

    win_width = w;
    win_height = h;

    return 1;
}

// ==============================================
// 保留旧函数（空实现，不报错）
// ==============================================
int DisplayWind::yuv420p_to_rgb888(AVFrame *, int, int, uint8_t *) {
    return 0;
}
