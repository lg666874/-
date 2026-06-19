#ifndef IJKMEDIAPLAYER_H
#define IJKMEDIAPLAYER_H

#include <mutex>
#include <thread>
#include <functional>
#include "ff_ffplay.h"
#include "ffmsg_queue.h"
#include "string.h"
#include "ff_ffplay_def.h"
#include <QKeyEvent>
/*-
 * ijkmp_set_data_source()  -> MP_STATE_INITIALIZED
 *
 * ijkmp_reset              -> self
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_IDLE               0

/*-
 * ijkmp_prepare_async()    -> MP_STATE_ASYNC_PREPARING
 *
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_INITIALIZED        1

/*-
 *                   ...    -> MP_STATE_PREPARED
 *                   ...    -> MP_STATE_ERROR
 *
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_ASYNC_PREPARING    2

/*-
 * ijkmp_seek_to()          -> self
 * ijkmp_start()            -> MP_STATE_STARTED
 *
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_PREPARED           3

/*-
 * ijkmp_seek_to()          -> self
 * ijkmp_start()            -> self
 * ijkmp_pause()            -> MP_STATE_PAUSED
 * ijkmp_stop()             -> MP_STATE_STOPPED
 *                   ...    -> MP_STATE_COMPLETED
 *                   ...    -> MP_STATE_ERROR
 *
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_STARTED            4

/*-
 * ijkmp_seek_to()          -> self
 * ijkmp_start()            -> MP_STATE_STARTED
 * ijkmp_pause()            -> self
 * ijkmp_stop()             -> MP_STATE_STOPPED
 *
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_PAUSED             5

/*-
 * ijkmp_seek_to()          -> self
 * ijkmp_start()            -> MP_STATE_STARTED (from beginning)
 * ijkmp_pause()            -> self
 * ijkmp_stop()             -> MP_STATE_STOPPED
 *
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_COMPLETED          6

/*-
 * ijkmp_stop()             -> self
 * ijkmp_prepare_async()    -> MP_STATE_ASYNC_PREPARING
 *
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_STOPPED            7

/*-
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_ERROR              8

/*-
 * ijkmp_release            -> self
 */
#define MP_STATE_END                9

class IjkMediaPlayer
{
public:
    IjkMediaPlayer();
    ~IjkMediaPlayer();

    int ijkmp_create(std::function<int(void *)> msg_loop);
    int ijkmp_destroy();
    int ijkmp_set_data_source(const char *url);
    int ijkmp_prepare_async();
    int ijkmp_msg_loop(void *arg);
    void AddVideoRefreshCallback(std::function<int(const Frame *)> callback);//现将每一帧图像的回调存储起来，之后调用再处理函数内容
    int ijkmp_get_msg(AVMessage *msg,int block);
    int ijkmp_start();
    int ijkmp_stop();
    int ijkmp_pause();//进行暂停
    int ijkmp_step();
    int ijkmp_volume();
    int ijkmp_play_progress();
    void EVENT_LOOP();
    int ijkm_speed();
    int ijkm_muted();
    int ijkmp_screenshot(char *file_path);

//    int ijkmp_seek_to(long msec);
//    int ijkmp_get_state();
//    int ijkmp_is_playing();
//    long ijkmp_get_current_position();
//    long ijkmp_get_duration();
//    long ijkmp_get_playable_duration();
//    void ijkmp_set_loop();
//    int ijkmp_get_loop();

//    //设置音量
//    void ijkmp_set_playback_volume(float volume);
    //EVENT_LOOP线程
    std::thread *event_loop_  = NULL;
    //更改播放源
    int change_source = 0;
private:
    //互斥量
    std::mutex mutex_;
    //真正的播放器对象
    FFPlayer *ffplayer_ = NULL;
    //创建msg_loop函数指针句柄
    std::function<int(void*)>msg_loop_ = NULL;
    //消息机制线程
    std::thread *msg_thread_;
    //播放资源对应url
    char *data_source_;

    int mp_state_;//播放状态


};
void  updata_volume(FFPlayer *is,int sign,double step);
void event_loop(FFPlayer *cur_stream);
void refresh_loop_wait_event(FFPlayer *cur_stream,SDL_Event *event);
void stream_seek(FFPlayer *is,int64_t pos,int64_t rel,int seek_flags);
#endif // IJKMEDIAPLAYER_H
