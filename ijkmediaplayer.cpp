#include "ijkmediaplayer.h"
#include "ff_ffplay.h"
#include <iostream>
#include <QDebug>

#include "ctrlbar.h"
extern double slider_volume;
extern double slider_play;
IjkMediaPlayer::IjkMediaPlayer()
{

}
IjkMediaPlayer::~IjkMediaPlayer()
{
    if(event_loop_&&event_loop_->joinable()){
        event_loop_->join();//等待线程退出
    }
   delete event_loop_;
   event_loop_ = NULL;

}

int IjkMediaPlayer::ijkmp_create(std::function<int(void *)> msg_loop)
{
    int ret = 0;
    ffplayer_ = new FFPlayer();//在这里进行开始，创建全局ffplayer播放器
    if(!ffplayer_)//创建分配失败
    {
      //  std::cout << " new FFplayer() failed\n ";
        printf("new ffplayer()failed\n");
        return -1;
    }
    //将传入的msg_loop进行装入msg_loop_容器
    msg_loop_ = msg_loop;//传递函数标识符以及函数句柄进行代理给msg_loop_
    ret = ffplayer_->ffp_create();//其实就是msgqueue的初始化
    if(ret < 0)
    {
        return -1;
    }
    return 0;
}
int IjkMediaPlayer::ijkmp_destroy()
{
    ffplayer_->ffp_destroy();
    //释放msg_loop刷新线程
    if(msg_thread_&&msg_thread_->joinable()){
        msg_thread_->detach();//等待线程退出
    }
    return 0;
}
int IjkMediaPlayer::ijkmp_set_data_source(const char *url)
{
    if(!url)
    {
        //在没有数据是已经结束了
        return -1;
    }
    //将内存拷贝到IjkMediaPlayer对象中，并把指针进行赋予data_source_，data_source所占用的内存等同于源总文件大小
    data_source_ = strdup(url);
    return 0;
}



int IjkMediaPlayer::ijkmp_prepare_async()
{
    //判断mp的状态
    //正在准备中
//    if(!change_source){
     mp_state_ = MP_STATE_ASYNC_PREPARING;
     //启动消息队列，调用的是ffmsg_queue中的函数，这属于跨文件函数调用，不属于对象函数调用，但是在这个IjkMediaPlayer本类之中还是可以进行对类成员进行操作
     msg_queue_start(&ffplayer_->msg_queue_);
     //创建循环线程ijkmp_msg_loop,线程入口函数仅仅作"中间站函数调用"
     msg_thread_ = new std::thread(&IjkMediaPlayer::ijkmp_msg_loop,this,this);
     //调用ffplayer进行播放数据
//     }
//    if(change_source){
//      ffplayer_->change_source = 1;
//      change_source = 0;
//    }
     int ret  = ffplayer_->ffp_prepare_async_l(data_source_);//播放器要启动了
     if(ret < 0)
     {
         mp_state_ =  MP_STATE_ERROR;
         return -1;
     }


     return 0;
}
int IjkMediaPlayer::ijkmp_msg_loop(void *arg)
{
//    std::cout<<"55555555555555555"<<std::endl;
//   event_loop(ffplayer_);
    msg_loop_(arg);//函数对象容器（存储了绑定后的 MainWind::message_loop，是 “中转的目标”），在调用msg_loop_(arg)其实就是在调用MainWind::message_loop
    return 0;
}
int IjkMediaPlayer::ijkmp_start()
{
    //一切准备就绪，可以开始了
// qDebug() << "666666666666666666666666";
    ffp_notify_msg1(ffplayer_,FFP_REQ_START);
}

int IjkMediaPlayer::ijkmp_get_msg(AVMessage *msg,int block)//就执行一次肯定卡在这个函数里面去了
{ //只不过将IjkMediaPlayer这个类中的成员ffplayer_调用放在ffplayer_类成员的msg_queue_里，ffplayer_类依旧是真个播放器状态管理核心
    int64_t pos = 0;
    static int last_volume = 0;
    while(1)
   {    /*std::cout<<"222222222222222222222222";//播放器准备完成*/
//          qDebug() << "hello world11111111111111111111!";//事实证明两个函数都可以进行打印
       int cotinue_wait_next_msg = 0;
       int retval = msg_queue_get(&ffplayer_->msg_queue_,msg,block);//从msg_queue_中进行获取消息然后传出去
       if(retval <= 0 )
       {
           return retval;
       }
       //先自己判断一遍，然后判断结束后传出去

       switch(msg->what){
       //在调用mp->ijkmp_start();时会发送信号FFP_REQ_START，
       case FFP_REQ_START://这个FFP_REQ_START信号来自message_loop中的ijk_start
           std::cout<<__FUNCTION__<<"FFP_MSG_PREPARED"<<std::endl;//播放器准备完成
           cotinue_wait_next_msg = 1 ;//只会触发一次，后面就是msgloop进行
           //可以调用ffplayer播放器进行播放开始
        //虽然是IjkMediaPlayer类嵌套但是发挥主导作用的是ffplayer
           ffplayer_->ffp_start_l();//这个start才是播放器真正的start,这个start进行执行相应的开始功能，将暂停置为0(开始)
           SDL_Init(SDL_INIT_EVENTS);
           event_loop_ = new std::thread(&IjkMediaPlayer::EVENT_LOOP,this);
           goto msg_free;
           break;   
       case FFP_REQ_PAUSE:
           //mp->ffplayer_->pause_ = !mp->ffplayer_->pause_;
//            std::cout<<"0000000000000000:"<<ffplayer_->pause_<<std::endl;
           ffplayer_->pause_ = !ffplayer_->pause_;
//           std::cout<<"1111111111111111:"<<ffplayer_->pause_<<std::endl;
           break;
       case FFP_REQ_VOLUME:
           ffplayer_->volume  =  slider_volume/100*128;
           break;
       case FFP_REQ_SEEK:
//           ffplayer_->volume  =  slider_play/100*128;
            pos = slider_play/100*ffplayer_->ic->duration/20;
//            std::cout<<"slider66999999999:"<<slider_play<<std::endl;
//        pos =  50/100*ffplayer_->ic->duration/20;
//          pos = ffplayer_->now_pts/20 + 10/100*ffplayer_->ic->duration/20;
           stream_seek(ffplayer_,pos,ffplayer_->seek_rel,0);
//           ffplayer_->seek_req = 1;
//           ffplayer_->ts = slider_play/100*ffplayer_->ic->duration/20;//单位微妙
//           ffplayer_->min_ts = ffplayer_->ts - ffplayer_->seek_rel;
//           ffplayer_->max_ts = ffplayer_->ts + ffplayer_->seek_rel;
           break;
       case FFP_REQ_STEP:
           ffplayer_->step = 1;
           break;
       case FFP_REQ_MUTED:
            std::cout<<"1111111111111111:"<<std::endl;
           ffplayer_->muted = !ffplayer_->muted;
           if(ffplayer_->muted)
           {
               last_volume = ffplayer_->volume;//在每次静音时进行获取音量
               ffplayer_->volume = 0;
           }
           else
               ffplayer_->volume = last_volume;//那么在下次接除静音时就可以获得获得上次的音量，这两个状态来回切换
           break;
       case FFP_REQ_SPEED:
               ffplayer_->speed_flag = !ffplayer_->speed_flag;
               if(ffplayer_->speed_flag)
               {
                   ffplayer_->speed = 2.0;
               }
               else ffplayer_->speed = 1.0;
               ffplayer_->ffp_set_playback_rate(ffplayer_->speed);
               break;
      case  FFP_REQ_SCREENSHOT:
           ffplayer_->ffp_screenshot_l((char *)msg->obj);
           break;
//          std::cout<<"1111111111111111:"<<ffplayer_->pause_<<std::endl;
          /*  updata_volume(ffplayer_,1,(slider_volume - ffplayer_->volume));*///这里的20是人耳音频水平，也是人类听觉的百分比100%
 //默认情况下：
        default:
//           std::cout<<__FUNCTION__<<"default"<<msg->what<<std::endl;//如果是其他消息就撑场打印default和消息名称
            std::cout<<"ijkmp_get_msg return default:"<<msg->what<<std::endl;//如果是其他消息就撑场打印default和消息名称
//           break;
           return 1;
       }
msg_free:
       if(cotinue_wait_next_msg)
       {
           //只有FFP_REQ_START这个信号接收到时才会在这里去释放消息
           msg_free_res(msg);
//           continue;//然后继续重新判断，不满足跳出，只是在这个小循环里
           cotinue_wait_next_msg =  0;
       }

   }
}

void IjkMediaPlayer::EVENT_LOOP(){//事件线程循环
   std::cout<<"55555555555555555"<<std::endl;
      event_loop(ffplayer_);
}

#define REFRESH_RATE 0.35
void refresh_loop_wait_event(FFPlayer *cur_stream,SDL_Event *event){
    double remaining_time = 0.0; /* 休眠等待，remaining_time的计算在video_refresh中 */
    /* 调用SDL_PeepEvents前先调用SDL_PumpEvents，将输入设备的事件抽到事件队列中 */
//    SDL_PumpEvents();
    /*
     * SDL_PeepEvents check是否事件，比如鼠标移入显示区等
     * 从事件队列中拿一个事件，放到event中，如果没有事件，则进入循环中
     *///SDL_PollEvent(event)
    while (!SDL_PeepEvents(event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT)) {
//        if (!cursor_hidden && av_gettime_relative() - cursor_last_shown > CURSOR_HIDE_DELAY) {
//            SDL_ShowCursor(0);
//            cursor_hidden = 1;
//        }
        /*
         * remaining_time就是用来进行音视频同步的。
         * 在video_refresh函数中，根据当前帧显示时刻(display time)和实际时刻(actual time)计算需要sleep的时间，保证帧按时显示
         */
        if (remaining_time > 0.0)
            av_usleep((int64_t)(remaining_time * 1000000.0));
        remaining_time = REFRESH_RATE;//这个是休眠
//        cur_stream->pause_ = !cur_stream->pause_;
//        if (is->show_mode != SHOW_MODE_NONE && (!is->paused || is->force_refresh))
//            video_refresh(is, &remaining_time);
//        SDL_PumpEvents();
    }
    //  double remaining_time = 0.0;
//     SDL_PumpEvents();
//   if(!SDL_PeepEvents(event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT)) {
//    if (remaining_time > 0.0)
//        av_usleep((int64_t)(remaining_time * 1000000.0));
//    remaining_time = REFRESH_RATE;//这个是休眠
//    SDL_PumpEvents();
//   }
}
void event_loop(FFPlayer *cur_stream){
    SDL_Event event ;
    static int64_t progress = 0;
    int64_t pos = 0;
    int64_t max_pos =  cur_stream->ic->duration/20;
    int64_t delta_pos = 10/100*cur_stream->ic->duration/20;

    for(;;){
    refresh_loop_wait_event(cur_stream, &event);//是这个API在不断监听键盘，鼠标事件，等待监听取出事件
    if(event.type == SDL_KEYDOWN){//如果按键触发

    switch (event.key.keysym.sym) {//根据按键输入办事
    case SDLK_UP:
        if(cur_stream->volume == SDL_MIX_MAXVOLUME)
            cur_stream->volume = SDL_MIX_MAXVOLUME - 10;

            cur_stream->volume += 10;

        if(cur_stream->volume >= SDL_MIX_MAXVOLUME - 10)
            cur_stream->volume = SDL_MIX_MAXVOLUME;//超绝算法！！！
        break;     // 键盘【上】方向键
    case SDLK_DOWN:
        if( cur_stream->volume == 0)
            cur_stream->volume = 10;

            cur_stream->volume -= 10;

        if(cur_stream->volume <= 10)
            cur_stream->volume = 0;
        break;     // 键盘【下】方向键
    case SDLK_LEFT:
        progress -= 10;
//        pos = progress/100*cur_stream->ic->duration/20;
        pos = (cur_stream->now_pts - 10000000)/20;
        stream_seek(cur_stream,pos,cur_stream->seek_rel,0);
        break;    // 键盘【左】方向键slider_play/100*ffplayer_->ic->duration/20
    case SDLK_RIGHT:
//        pos = cur_stream->now_pts/20 + 20/100*cur_stream->ic->duration/20  /*+ 10/100*cur_stream->ic->duration*/;
        progress += 10;
//        pos = progress/100*cur_stream->ic->duration/20;
        pos = (cur_stream->now_pts + 10000000)/20;
//        if(pos == max_pos)
//            pos = max_pos - delta_pos;

//            pos += delta_pos;

//        if(pos >= max_pos - delta_pos)
//            pos = max_pos;//超绝算法！！！
        stream_seek(cur_stream,pos,cur_stream->seek_rel,0);
        break;
    case SDLK_SPACE:
        cur_stream->pause_ = !cur_stream->pause_;
        break;
     default:
        break;

   }
  }
 }
}


void  updata_volume(FFPlayer *is,int sign,double step)
{
    double volume_level = is->volume ? (20*log(is->volume / (double)SDL_MIX_MAXVOLUME)/log(10))  : -1000.0;
    int new_volume = lrint(SDL_MIX_MAXVOLUME*pow(10.0,(volume_level + step*sign)/20.0));
    is->volume = av_clip(is->volume == new_volume ? (is->volume + sign): new_volume,0,SDL_MIX_MAXVOLUME);

}
void stream_seek(FFPlayer *is,int64_t pos,int64_t rel,int seek_flags)
{
    SDL_LockMutex(is->wait_mutex);
    is->seek_req = 1;
    is->seek_flags = seek_flags;
    is->ts  =  pos ;//单位微妙
    is->min_ts = pos  - rel;
    is->max_ts = pos  +  rel;
    SDL_CondSignal(is->wait_read_thread);
    SDL_UnlockMutex(is->wait_mutex);
}
int IjkMediaPlayer::ijkmp_stop()
{
     int retval = ffplayer_->ffp_stop_l();//利用request=1进行将线程停止
     if(retval < 0){
     return retval;
     }

}
void IjkMediaPlayer::AddVideoRefreshCallback(std::function<int(const Frame *)>callback)
{
    ffplayer_->AddVideoRefreshCallback(callback);//其实视频刷新回调函数是在ffplayer播放器中实现的
}

int IjkMediaPlayer::ijkmp_pause()
{ //调用ffplay播放器将事件传递给ffplay的msgqueue队列中，之后在消息处理线程中进行响应达到控制播放器的目的
    ffp_notify_msg1(ffplayer_,FFP_REQ_PAUSE);
//    ffplayer_->pause_ = !ffplayer_->pause_;
}
int IjkMediaPlayer::ijkmp_step()
{
    ffp_notify_msg1(ffplayer_,FFP_REQ_STEP);
}
int IjkMediaPlayer::ijkmp_volume()
{
    ffp_notify_msg1(ffplayer_,FFP_REQ_VOLUME);
}
int IjkMediaPlayer::ijkmp_play_progress()
{
    ffp_notify_msg1(ffplayer_,FFP_REQ_SEEK);
}
int IjkMediaPlayer::ijkm_speed()
{
    ffp_notify_msg1(ffplayer_,FFP_REQ_SPEED);
}
int IjkMediaPlayer::ijkm_muted()
{
    ffp_notify_msg1(ffplayer_,FFP_REQ_MUTED);
}
// 请求截屏
int IjkMediaPlayer::ijkmp_screenshot(char *file_path)
{
    ffp_remove_msg(ffplayer_, FFP_REQ_SCREENSHOT);
    ffp_notify_msg4(ffplayer_, FFP_REQ_SCREENSHOT, 0, 0, file_path, strlen(file_path) + 1);
    return 0;
}

//int IjkMediaPlayer::ijkmp_seek_to(long msec)
//{

//}
//int IjkMediaPlayer::ijkmp_get_state()
//{

//}
//int IjkMediaPlayer::ijkmp_is_playing()
//{

//}
//long IjkMediaPlayer::ijkmp_get_current_position()
//{

//}
//long IjkMediaPlayer::ijkmp_get_duration()
//{

//}
//long IjkMediaPlayer::ijkmp_get_playable_duration()
//{

//}
//void IjkMediaPlayer::ijkmp_set_loop()
//{

//}
//int IjkMediaPlayer::ijkmp_get_loop()
//{

//}

///**/
////设置音量
//void IjkMediaPlayer::ijkmp_set_playback_volume(float volume)
//{

//}

