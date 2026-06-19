#include "mainwind.h"
#include "ui_mainwind.h"
#include <thread>
#include <functional>
#include <iostream>
#include <QDebug>
#include <QDateTime>
#include "ffmsg_queue.h"
#include "ff_ffmsg.h"
#include "ijkmediaplayer.h"
#define YUV_FORMAT SDL_PIXELFORMAT_IYUV
#define YUV_WIDTH 320
#define YUV_HEIGHT 240

MainWind::MainWind(QWidget *parent) ://构造函数
    QMainWindow(parent),//继承QWidget参数
    ui(new Ui::MainWind)
{
    ui->setupUi(this);
    //初始化信号槽
    InitSignalsAndSlots();
    play_name1 = (char*)malloc(20);
    //确保窗口能获取键盘焦点，不漏任何按键
        this->setFocusPolicy(Qt::StrongFocus);
        this->setFocus();
//    this->setFocusPolicy(Qt::NoFocus);

}
void MainWind::renderOneFrame()
{
//    // 模拟YUV420P数据（灰色帧)
//       int video_width = YUV_WIDTH;
//       int video_height = YUV_HEIGHT;
//       int win_width = YUV_WIDTH;
//       int win_height = YUV_HEIGHT;
//       int ySize = video_width*video_height;//一帧总大小
//       int uvSize = ySize / 4;//UV各分量大小
//       int yuv_frame_Size = ySize + 2 * uvSize;
//       uint8_t *data = (uint8_t*)malloc(yuv_frame_Size);//分配单帧长度大小
//       memset(data, 0, yuv_frame_Size);
//       SDL_Rect rect;
//       FILE *video_fd = NULL;
//       const char *yuv_path = "tsxk3.yuv";
//       size_t video_buff_len = 0;//每次读取的大小
//       video_fd = fopen(yuv_path,"rb");
//        video_buff_len = fread(data,1,yuv_frame_Size,video_fd);//打开这个video_fd 文件，开始读取，读取到的数据放到video_buf中，以1个字节一个字节的传输，传输总长度为yuv_frame_len这么长

//       //显示区域，可以通过修改w和h进行缩放
//       rect.x = 0;//rect作用于渲染器renderer
//       rect.y = 0;
//       float w_ratio = win_width * 1.0 / video_width;
//       float h_ratio = win_height * 1.0 / video_height;
//       //根据rect的x，y坐标进行渲染，全部图像渲染
//       rect.w = video_width * w_ratio ;  //通过修改win_width和win_height去改变视频播放比例大小
//       rect.h = video_height * h_ratio;  //高度缩放

//       // SDL渲染核心步骤，本质上是通过更新纹理，然后给渲染器进行显示

//       //清除渲染器
//       SDL_RenderClear(sdlRenderer);
//       //更新纹理进行终端显示
//       SDL_UpdateTexture(sdlTexture, nullptr, data, YUV_WIDTH);
//       //将纹理拷贝给渲染器
//       SDL_RenderCopy(sdlRenderer, sdlTexture, nullptr, &rect);
//       //渲染器进行显示
//       SDL_RenderPresent(sdlRenderer);
//       //释放数据
//       free(data);
}

MainWind::~MainWind()
{

    delete ui;
}

int MainWind::InitSignalsAndSlots()
{
    connect(ui->ctrlBarWind,&CtrlBar::SigPlayOrPause,this,&MainWind::OnPlay);
    connect(ui->ctrlBarWind,&CtrlBar::Stop,this,&MainWind::OnStop);
    connect(ui->ctrlBarWind,&CtrlBar::Pause,this,&MainWind::OnPause);
    connect(ui->ctrlBarWind,&CtrlBar::Step,this,&MainWind::OnStep);
    connect(ui->ctrlBarWind,&CtrlBar::Change_volume,this,&MainWind::OnChange_volume);
//    connect(ui->ctrlBarWind,&CtrlBar::Change_volume,this,&MainWind::screenshot_);
    connect(ui->ctrlBarWind,&CtrlBar::Change_play,this,&MainWind::OnChange_play);
    connect(ui->ctrlBarWind,&CtrlBar::Change_speed,this,&MainWind::OnChange_speed);
//    connect(ui->ctrlBarWind,&CtrlBar::Change_muted,this,&MainWind::OnChange_muted);
    connect(ui->ctrlBarWind,&CtrlBar::Screenshot_,this,&MainWind::screenshot_);
   // ui->progressSlider->setValue()

//    connect(ui->TitleWid,&Title::Decrese_volume,this,&MainWind::screenshot_);
//    connect(ui->TitleWid,&Title::Add_volume,this,&MainWind::Add_volume_);
//    connect(ui->TitleWid,&Title::go_ahead,this,&MainWind::go_ahead_);
//    connect(ui->TitleWid,&Title::go_back,this,&MainWind::go_back_);



    connect(ui->PlaylistContents, &PlayListWind::playItemChanged, this, [=](const QString &item) {//正确的 Qt Lambda 信号槽格
        // 方式1：Qt 推荐的 qDebug() 输出（无编码问题）
        qDebug() << "选中的播放项：" << item;
//       play_name = （char*）malloc(10);
//       memset(play_name,0,10);
        // 方式2：如果非要用 std::cout，需转成 std::string
        std::cout <<item.toStdString()<< std::endl;
        std::string play_name0 = item.toStdString();


        // 按字符串长度+1（\0）动态分配

        if (!play_name1) {
            qDebug() << "malloc 失败";
            return;
        }
        memset(play_name1, 0, 20);
        memcpy(play_name1, play_name0.data(), play_name0.size());
//       memcpy(play_name,play_name0.data(),10);
        play_name = play_name1;
        std::cout <<play_name<< std::endl;
      int ret =  strcmp(play_name,"dldl.mp4");//strcmp用来判断两个字符串是否相等一模一样
      std::cout <<ret<< std::endl;
      static double get_clk_count = 0;
      if(av_gettime_relative()/1000000.0 - get_clk_count <= 0.5000000){
         OnPlay();
        get_clk_count = 0;
      }
      else{
          get_clk_count = av_gettime_relative()/1000000.0;//转化为秒
      }
//      if(){

//      }

        // 你的业务逻辑（比如赋值给 play_name）
        // play_name = item; // 注意：play_name 若为 QString 类型，直接赋值，不要用 &（避免野指针）
    });
}
void MainWind::keyPressEvent(QKeyEvent *event)
{
    static double last_get_event = 0;
    // 1. 初始化SDL事件结构体，清空脏数据（必加，防止SDL事件异常）
    SDL_Event sdlevent;
    SDL_zero(sdlevent);
    // 2. 设置SDL事件类型为「按键按下」，和SDL原生捕获一致
    sdlevent.type = SDL_KEYDOWN;

    // 3.关键：Qt按键 ↔ SDL按键 一对一精准映射（所有按键全生效）
    switch (event->key())
    {
        // 你的核心按键：A键、空格、上下左右方向键（完全对应你的SDL逻辑）
        case Qt::Key_A:        sdlevent.key.keysym.sym = SDLK_a;        break;
        case Qt::Key_Space:    sdlevent.key.keysym.sym = SDLK_SPACE;    break;
        case Qt::Key_Left:     sdlevent.key.keysym.sym = SDLK_LEFT;     break;
        case Qt::Key_Right:    sdlevent.key.keysym.sym = SDLK_RIGHT;    break;
        case Qt::Key_Up:       sdlevent.key.keysym.sym = SDLK_UP;       break;
        case Qt::Key_Down:     sdlevent.key.keysym.sym = SDLK_DOWN;     break;
        // 可选补充：ESC退出键（播放器必备）
        case Qt::Key_Escape:   sdlevent.key.keysym.sym = SDLK_ESCAPE;   break;
        // 其他按键无需转发，直接返回
        default: return;
    }

    // 4. 核心操作：将映射后的按键事件，推入SDL事件队列
//    if(av_gettime_relative()/1000000.0 - last_get_event <= 0.500000)
//            last_get_event = av_gettime_relative()/1000000.0;//转化为秒
/*      else  */
       SDL_PushEvent(&sdlevent);


    // 5. 拦截Qt原生按键响应，避免Qt和SDL冲突
    event->ignore();
}
int MainWind::message_loop(void *arg)
{
    IjkMediaPlayer *mp = (IjkMediaPlayer *)arg;
    qDebug() << "message_loop start!!!" << endl;
    while(1)
    {
    AVMessage msg;
    //调用消息处理，没有消息则阻塞
    //这个ijkmp_get_msg函数非常关键，由于这个
//    qDebug() << "message_loop start!!!"<<std::endl;//事实证明两个函数都可以进行打印
    int retval = mp->ijkmp_get_msg(&msg,1);//取出来这个msg之后得进行调用才行,ijkmp_get_msg这个函数先执行一遍在默认退出
    //ijkmp_get_msg这个消息仅仅支持start
    if (retval <0){
         qDebug() << "ijkmp_get_msg failed!!!"<<endl;//事实证明两个函数都可以进行打印
         break;
        }
         qDebug() << "ijk_get_msg OK!"<<endl;//事实证明两个函数都可以进行打印
    //在从msg_queue中获取一帧msg消息，ijkmp_get_msg这个函数先对消息进行判断一遍再进行判断
    //这些消息来源来自
    switch (msg.what) {//剩下的消息过滤
    case FFP_MSG_FLUSH:
        qDebug()<< __FUNCTION__ << " FFP_MSG_FLUSH"<<endl;
    break;
   //在调用完stream_compont_open时进行发送信息FFP_MSG_PREPARED
    case FFP_MSG_PREPARED:
    std::cout << __FUNCTION__ << " FFP_MSG_PREPARED" <<std::endl;
    //拥有这个消息时开始启动播放
//     qDebug() << "666666666666666666666666";
    mp->ijkmp_start();//一切准备就绪，只是说刚刚stream_open，可以进行解码播放了，按图索骥、
    default:
    /*类似于：
     * ffplayer中readthread线程发送FFP_MSG_OPEN_INPUT
     *  FFP_MSG_FIND_STREAM_INFO
     * FFP_MSG_COMPONENT_OPEN之后发送FFP_MSG_PREPARED
     */
    std::cout << __FUNCTION__ << " default " << msg.what << std::endl;
//     qDebug() << "888888888888888888888!";//事实证明两个函数都可以进行打印
    break;
    }
    msg_free_res(&msg);
    qDebug() << "message_loop :" << mp;//打印2
    //先模拟线程运行
    std::this_thread::sleep_for(std::chrono::milliseconds(10));//线程消息不能等太久10ms
   }
    qDebug()<<"message_loop leave";
}
void MainWind::OnPause()
{
   mp_->ijkmp_pause();

}
void MainWind::OnStep()
{
   mp_->ijkmp_step();//现在时进行音量加10
//   qDebug()<<"value!!!"<<slider_volume;
}
void MainWind::OnChange_volume()
{
   mp_->ijkmp_volume();
   std::this_thread::sleep_for(std::chrono::milliseconds(100));//线程消息不能等太久10ms

}

void MainWind::OnChange_play()
{
   mp_->ijkmp_play_progress();
   std::this_thread::sleep_for(std::chrono::milliseconds(200));//线程消息不能等太久10ms

}
void MainWind::screenshot_()
{
    if(mp_) {
        QDateTime time = QDateTime::currentDateTime();
        // 比如 20230513-161813-769.jpg
        QString dateTime = time.toString("yyyyMMdd-hhmmss-zzz") + ".jpg";
        mp_->ijkmp_screenshot((char *)dateTime.toStdString().c_str());
    }
}
void MainWind::Add_volume_()
{
   mp_->ijkmp_step();
}
void MainWind::go_ahead_()
{
   mp_->ijkmp_step();
}
void MainWind::go_back_()
{
   mp_->ijkmp_step();
}
void MainWind::OnChange_speed()
{
   mp_->ijkm_speed();
}
void MainWind::OnChange_muted()
{
    mp_->ijkm_muted();
}
void MainWind::OnPlay()
{

    qDebug()<< "OnPlayOrPause call";
    int ret = 0;
    //1．先检测mp是否已经创建
    if(!mp_){
    //首先创建IjkMediaPlayer对象分配内存，主要是针对msg作用于UI界面于ffplayer播放器
    mp_ = new IjkMediaPlayer;
    //1.1 创建
    /* 在这里进行开始，创建全局ffplayer播放器
     *传递函数标识符以及函数句柄进行代理给msg_loop_
     * 初始化msg队列ffplayer类内成员msgqueue
    */
    ret= mp_->ijkmp_create(std::bind(&MainWind::message_loop, this, std::placeholders::_1));//UI界面处理逻辑
    if(ret < 0)
    {
    qDebug()<< "IjkMediaPlayer create failed";
    delete mp_;
    mp_= NULL;
    return;
    }
    //创建视频刷新输出回调函数OutputVideo，有这个函数就能进行回调了，给播放器注册视频刷新回调函数，具体ffplayer调用了callback就是调用了OutputVideo
    mp_->AddVideoRefreshCallback(std::bind(&MainWind::OutputVideo,this, std::placeholders::_1));
    //1.2 设置url
    //设置播放来源，文件地址
//  mp_->ijkmp_set_data_source("2_audio.mp4");
//    mp_->ijkmp_set_data_source("dldl.mp4");
    if(play_name==NULL)
    {
        char *default_playname = "rtmp://192.168.100.60:1935/live/01";
        play_name = default_playname;
    }
     mp_->ijkmp_set_data_source(play_name);
//    mp_->ijkmp_set_data_source("believe.mp4");
    //1.3 准备工作
    /* 启动消息队列，调用的是ffmsg_queue中的函数，这属于跨文件函数调用，不属于对象函数调用，但是在这个IjkMediaPlayer本类之中还是可以进行对类成员进行操作
     * 创建循环线程ijkmp_msg_loop,线程入口函数仅仅作"中间站函数调用"
     * 调用ffplayer进行播放数据
     * 调用FFPlayer中的ffp_prepare_async_l进行stream_open打开流
 stream_open进行:
         /*1.初始化SDL渲染视频，SDL播放音频，SDL定时器
          *2.初始化ffplayer对象中的音频和视频的packetqueue，framequeue
          *3.创建read_thread数据读取线程，从文件中读取数据
          *4.创建video_refresh_thread视频刷新线程进行调用MainWind::OutputVideo进行调用Draw函数进行视频刷新
          *error:调用stream_close
             */
    ret = mp_->ijkmp_prepare_async();
    //还好在很早之前就已经创建好了ijkmsgloop线程以及msg_queue,这也是最后可以通过一步一步stream_open进行不断发送消息为strem_compont_open作准备，然后
    //消息到位之后就开始播放，这个播放标志位来自于FFP_MSG_PREPARED，之后开始播放
    if(ret<0){
    qDebug() << "IjkMediaPlayer create failed";
    delete mp_;
    mp_ = NULL;
    return;
    }
    }else{
    //已经准备好了，则暂停或者恢复播放
    qDebug() << "IjkMediaPlayer create SUCCESS";
//    mp_->ijkmp_set_data_source(play_name);//获取播放源
//    mp_->change_source = 1;
//    mp_->ijkmp_prepare_async();
}

}
void MainWind::OnStop()
{

     qDebug()<<"OnStop call";
     if(mp_){
         /*1.将ffplayer对象中的成员abort_request = 1;请求退出
          *2.将meg_queue队列中的成员abort_request = 1
          *3.唤醒msg_queue队列
          */
         mp_->ijkmp_stop();//目的将msg_queue中的abort_request = 1置一以及将abort_request = 1;//请求标志位置一退出，这个标志位是全局的
         //这个线程停止是没有问题的
         /*1.摧毁队列
          *2.调用stream_close:
            /*1.abort_request = 1;//请求退出
             *2.
             *
          *
          */
         mp_->ijkmp_destroy();//调用stream_close与摧毁消息队列

//         delete mp_;
//         mp_=NULL;
//         free(play_name1);
     }
}
int MainWind::OutputVideo(const Frame *frame)//输出视频帧
{
    return ui->showWind->Draw(frame);
}
