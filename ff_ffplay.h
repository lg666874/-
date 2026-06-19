#ifndef FF_FFPLAY_H
#define FF_FFPLAY_H

#include <thread>
#include "ffmsg_queue.h"
#include "string.h"
#include "ff_ffmsg.h"
#include "ff_ffplay_def.h"
#include <functional>
#include "sonic.h"
#include "screenshot.h"
class Decoder
{
public:
    AVPacket pkt_ = {0};
    AVPacket flush_pkt = {0};
        PacketQueue	*queue_;         // 数据包队列
        AVCodecContext	*avctx_;     // 解码器上下文
        bool pause_d = true;
        int		pkt_serial_ = 0;         // 包序列
        int seek_for_set_audio_clock_ = 0;
        int		finished_ = 0;           // =0，解码器处于工作状态；=非0，解码器处于空闲状态
//        int		packet_pending;     // =0，解码器处于异常状态，需要考虑重置解码器；=1，解码器处于正常状态
        std::thread *decode_thread_ = NULL;
//        std::thread *video_decode_thread_ = NULL;
        Decoder();
        ~Decoder();
        void decoder_init(AVCodecContext *avctx,PacketQueue *queue);//根据解码器上下文创建解码器
        //创建和启动线程
        int decoder_start(enum AVMediaType codec_type,const char *thread_name,void *arg);
        //停止解码线程
        void decoder_abort(FrameQueue *fq);
        void decoder_destroy();
        int decoder_decode_frame(AVFrame *frame);
        int get_video_frame(AVFrame *frame);
        int queue_picture(FrameQueue *fq,AVFrame *src_frame,double pts,
                          double duration,int64_t pos, int serial);
        int audio_thread(void *arg);
        int video_thread(void *arg);
        SDL_cond	*empty_queue_cond;  // 检查到packet队列空时发送 signal缓存read_thread读取数据
        int64_t		start_pts = 0;          // 初始化时是stream的start time
        AVRational	start_pts_tb;       // 初始化时是stream的time_base
        int64_t		next_pts = 0;           // 记录最近一次解码后的frame的pts，当解出来的部分帧没有有效的pts时则使用next_pts进行推算
        AVRational	next_pts_tb;        // next_pts的单位
        SDL_Thread	*decoder_tid;       // 线程句柄

};

class FFPlayer
{

public:
    FFPlayer();
    int ffp_create();
    int ffp_destroy();
    int ffp_prepare_async_l(char *filename);

    //播放控制
    int ffp_start_l();
    int ffp_stop_l();
    int stream_open(const char *file_name);
    int stream_close();
    //打开指定steam对应解码器，创建解码线程，以及初始化对应的输出
    int stream_component_open(int stream_index);
    //关闭指定steam的解码线程，释放解码器资源
    int stream_component_close(int stream_index);

    int start();

    int audio_open(int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate,struct AudioParams *audio_hw_params);
    void audio_close();
    double video_refresh(double *remaining_time);//视频刷新
    int video_refresh_thread();//视屏刷新线程，刷新视频调用video_fresh
    std::thread *video_refresh_thread_ = NULL;
    std::function<int(const Frame*)> video_refresh_callback_ = NULL;
    void AddVideoRefreshCallback(std::function<int(const Frame *)> callback);
    MessageQueue msg_queue_ ;//在ffplay中加上指针挺好用的,从ffplay播放器中进行时刻响应时间进行异步播放
    char *input_filename_;
    int read_thread();//创建read线程读取fd文件中的stream流

    //时钟函数
    int init(Clock *c);//初始化时钟结构体,就是将时钟的pts置为NAN
    void setClock(Clock *c,double pts);//设置时钟时间戳并记录当时的时间
    void setClock_at(Clock *c,double pts,double time);//设置时钟时间，将参数赋予给时钟
    double get_clock(Clock *c);//获取返回时钟时间
    //屏幕截取
    int ffp_screenshot_l(char *screen_path);
    void screenshot(AVFrame *frame);
    //倍数播放
    void ffp_set_playback_rate(float rate);

    float ffp_get_playback_rate();


    bool is_normal_playback_rate();


    int ffp_get_playback_rate_change();


    void ffp_set_playback_rate_change(int change);


    void ffp_set_playback_volume(int value);

    void check_play_finish();

    int get_target_frequency();

    int get_target_channels();


    //帧队列
    FrameQueue pictq;//视频Frame队列
    FrameQueue sampq;//采样Frame队列
    //解码器
    Decoder auddec;
    Decoder viddec;
    //包队列
    PacketQueue audioq;
    PacketQueue videoq;
    int abort_request = 0;
    int			audio_write_buf_size;
    AVStream *audio_st = NULL;//音频流
    AVStream *video_st = NULL;//视频流

    int audio_stream = -1;
    int video_stream = -1;

//    SDL_mutex wait_mutex;
    std::thread *continue_read_thread = NULL;

    int eof = 0;
    AVFormatContext *ic = NULL;

    //音频输出相关
    int volume = 100;
    int muted = 0;
    int speed_flag = 0;
    double speed = 1.0;
    int audio_hw_buf_size;
    struct AudioParams audio_src;//音频frame帧参数
    struct AudioParams audio_tag;//SDL支持的音频参数，重采样转换，audio_src->aduio_tag
    struct SwrContext *swr_ctx;         // 音频重采样context
    struct SwrContext* swr_hw_buf_size = 0; //SDL音频缓冲区的大小(字节为单位)
    //指向待播放的一帧音频数据，指向的数据区将被拷入SDL音频缓冲区，若经过重采样则指向audio_buf1
    //否则指向frame中的音频
    uint8_t *audio_buf = NULL; //指向需要重采样的数据
    uint8_t *audio_buf1 = NULL;//指向重采样之后的数据
    int audio_no_data = 0; //是否有音频数据
    int video_no_data = 0; //是否有视频数据
    unsigned int audio_buf_size = 0;//待播放的一帧音频数据(audio_buf指向)的大小
    unsigned int audio_buf1_size = 0;//申请到的音频缓冲区audio_buf1的实际尺寸
    int audio_buf_index = 0; //更新拷贝位置，当前音频帧中已拷入SDL音频缓冲区
    int step = 0;
    //seek相关
    int seek_req = 0;//这个是最主要的
    int64_t min_ts = 0;//根据ts通过算法计算min_ts
    int64_t ts = 0;//等于slider滑动时的大小
    int64_t max_ts = 0;//根据ts通过算法计算max_ts
    int seek_flags = 0;//默认以pts时间戳进行seek
    int64_t seek_rel = 0;
    int serial_ = 1;
    int is_realtime = 0;
    int seek_for_set_audio_clock = 0;
    //当前时间戳
    int64_t now_pts = 0;
    std::thread *read_thread_;

    //播放时钟设置
    Clock auddic = {0};//音频时钟
    double audio_clock = 0.0;
    int audio_clock_serial = 0;     // 播放序列，seek可改变此值, 解码后保存
    int64_t audio_callback_time_ = 0;
    bool pause_ = true;//自动转化为false or ture;
//    std::thread *video_refresh_thread_;

    //seek和pause共属的条件变量和锁mutex
    SDL_mutex *wait_mutex = NULL;
    SDL_cond  *wait_read_thread = NULL;
    int seek_single_step = 0;
    int is_audio = 0;
    int is_video = 0;

    int change_source = 0;
    int audio_change_source = 0;
    int video_change_source = 0;
    //截屏相关
    int req_screenshot_ = 0;
    char *screen_path_ = NULL;

    // 变速相关
    float       pf_playback_rate = 1.0;           // 播放速率
    int         pf_playback_rate_changed = 0;   // 播放速率改变
    // 变速相关
    sonicStreamStruct *audio_speed_convert = nullptr;
    int max_frame_duration = 3600;

//    //flush_pkt包
//    AVPacket flush_pkt = {0};





};

inline static void ffp_notify_msg1(FFPlayer *ffp, int what)
{
msg_queue_put_simple3(&ffp->msg_queue_, what, 0, 0);//调用ffplayer对象播放器中的msgqueue进行消息队列填充
}
inline static void ffp_notify_msg2(FFPlayer *ffp, int what, int arg1)
{
msg_queue_put_simple3(&ffp->msg_queue_, what, arg1,0);
}
inline static void ffp_notify_msg3(FFPlayer *ffp, int what, int arg1, int arg2)
{
msg_queue_put_simple3(&ffp->msg_queue_, what, arg1, arg2);
}
inline static void ffp_notify_msg4(FFPlayer *ffp, int what, int arg1, int arg2, void *obj, int obj_len)
{
    msg_queue_put_simple4(&ffp->msg_queue_, what, arg1, arg2, obj, obj_len);
}

inline static void ffp_remove_msg(FFPlayer *ffp, int what)
{
     msg_queue_remove(&ffp->msg_queue_,what);
}

#endif // FF_FFPLAY_H
