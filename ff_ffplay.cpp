#include "ff_ffplay.h"
#include <iostream>
#include "ijkmediaplayer.h"
#include "ff_ffplay_def.h"
#include <QDebug>

#include <iostream>
#include <cmath>
#include <string.h>
#include "sonic.h"
#include "screenshot.h"
/* Minimum SDL audio buffer size, in samples. */
#define SDL_AUDIO_MIN_BUFFER_SIZE 512
void print_error(const char *filename,int err)
{

}


FFPlayer::FFPlayer()
{
//    MessageQueue* q = (MessageQueue*)malloc(sizeof(MessageQueue));
}
int FFPlayer::ffp_create()//ijkMediaPlayer中的ffplay播放器进行操作
{
   std::cout << "ffp_create\n";
   msg_queue_init(&msg_queue_);//初始化msg队列ffplayer类内成员，成员已经创建
   return 0;
}

int FFPlayer::ffp_destroy()
{
    stream_close();
    //销毁消息队列
    msg_queue_destroy(&msg_queue_);//释放msg_queue_中的所有数据
    return 0;
}

int FFPlayer::ffp_prepare_async_l(char *file_name)
{
    int reval = 0;
    input_filename_ = strdup(file_name);
//    if(!change_source)
    reval = stream_open(file_name);

    return reval;
}
int FFPlayer::stream_open(const char *file_name)
{    //初始化SDL渲染视频，SDL播放音频，SDL定时器

    if (SDL_Init(SDL_INIT_VIDEO |SDL_INIT_AUDIO |SDL_INIT_TIMER ))
    {

//        av_log(NULL,AV__LOG_FATAL,"Did you set the DISPLAY variable?\n");
        return -1;
    }
    //初始化Frame帧队列

    if(frame_queue_init(&pictq,&videoq,VIDEO_PICTURE_QUEUE_SIZE) < 0)
    {
        goto fail;
    }
    if(frame_queue_init(&sampq,&audioq,SAMPLE_QUEUE_SIZE)<0)
    {
        goto fail;
    }
    //初始化Packet包队列
    if(packet_queue_init(&videoq)<0 ||packet_queue_init(&audioq)<0)
    {
            goto fail;
    }
    //初始化时钟
    init(&auddic);  
    //插件read_thread
    read_thread_  = new std::thread(&FFPlayer::read_thread,this);
    //创建视频刷新线程
    video_refresh_thread_ = new std::thread(FFPlayer::video_refresh_thread,this);



//    //创建read线程等待线程
//  continue_read_thread = new std::thread(FFPlayer::continue_read_thread,this);
     return 0;
fail:
    stream_close();
    return -1;
}
int FFPlayer::stream_close()
{
    //这个abort_request是因为特殊情况下出错会被置为1
   abort_request = 1;//每一个类有一个abort_request请求退出
   //3.释放视频刷新线程read_thread(进行读取文件pkt数据的线程)
   if(read_thread_ && read_thread_->joinable()){
              read_thread_->join(); //等待线程退出

}
  //5.关闭解复用器
      if(audio_stream >= 0)
      {
          stream_component_close(audio_stream);
      }
      if(video_stream >= 0)
      {
          stream_component_close(video_stream);
      }
      //2.队列释放
            //释放packet队列
//            packet_queue_destroy(&videoq);
//            packet_queue_destroy(&audioq);
//            //释放frame队列
//            frame_queue_destroy(&pictq);
//            frame_queue_destroy(&sampq);
//    4.释放视频刷新线程video_refresh_thread
           if(video_refresh_thread_&&video_refresh_thread_->joinable()){
                video_refresh_thread_->join();//等待线程退出
            }
         //1.进行释放文件名内存
            if(input_filename_)
        {
             free(input_filename_);
             input_filename_ = NULL;
        }


}

//如果想指定解码器怎么处理
/*有两个流，分别进行stream_component_open
 *一个是video流，另一个是Audio流
 *分别有两个avctx上下文，进行指导分配音频解码器和视频解码器
 * 所以此时通过switch调用函数实现了分叉实现函数性质
*/
int FFPlayer::stream_component_open(int stream_index)
{
     AVCodecContext *avctx;
     AVCodec *codec;
     int sample_rate;
     int nb_channels = 0;
     int64_t channel_layout;
     int ret = 0;



     //判断stream_index是否合法
     if(stream_index < 0 || stream_index >= ic->nb_streams)
         return -1;
     //为解码器分配一个编译器上下文结构体
     if(!change_source)
     avctx = avcodec_alloc_context3(NULL);
     if(!avctx)
       return AVERROR(ENOMEM);
     //将码流中的编码器信息拷贝到新分配的编解码器上下文结构体，将编解码器参数传递给上下文
     ret = avcodec_parameters_to_context(avctx,ic->streams[stream_index]->codecpar);
     if(ret < 0)
     goto fail;
     avctx->pkt_timebase = ic->streams[stream_index]->time_base;//将流timebase传递给上下文
     //根据codec_id查找解码器
     codec = avcodec_find_decoder(avctx->codec_id);
     if(!codec){
         ret = AVERROR(EINVAL);
         goto fail;
     }
     //根据解码器，获取解码器上下文，把 “配置好参数的编解码器上下文（AVCodecContext）” 和 “具体的编解码器（比如 H.264/AAC）”
     //绑定，完成编解码器的初始化（分配内存、校验参数、激活工作状态）(这是最为主要的作用)
     if((ret = avcodec_open2(avctx,codec,NULL))<0){
         goto fail;
     }

     switch (avctx->codec_type) {
     case AVMEDIA_TYPE_AUDIO:
         //根据解码器上下文获取音频格式参数
         sample_rate = avctx->sample_rate;//采样率
         nb_channels = avctx->channels;//通道数
         channel_layout = avctx->channel_layout;//通道布局
         /*prepare audio output准备音频输出
          *调用audio_open打开sdl音频输出，实际打开的设备参数保存在audio_tag，返回表示输出设备的缓冲区大小
          *在解码线程audio_thread音频解码线程创建之前就已经将满足SDL音频播放器参数转化成ffmpeg能够理解的参数即audio_tag这个结构体
          *其实wanted_spec满足的格式就是s16加上源avctx参数，但要翻译成ffmpeg能理解的参数audio_hw_params（audio_tag）
*/
         //先解码取帧再进行重采样符合声音播放器的格式，再进行播放
         if((ret = audio_open(channel_layout,nb_channels,sample_rate,&audio_tag)) < 0)
             goto fail;
         audio_hw_buf_size = ret;//返回实际比如2帧数据，一帧就是1024个采样点，1024*2*2 *2= 8192字节
         audio_src = audio_tag;//暂且将数据源参数等同于目标输出参数，所以重采样后的数据格式都应该像audio_tag结构体一样，才能保证sdl进行正常音频播放
         //初始化audio_buf相关参数
         audio_buf_size = 0;
         audio_buf_index = 0;

         audio_stream = stream_index;//获取audio的stream索引，等于1
         audio_st = ic->streams[stream_index];//获取audio的stream指针
         //初始化ffplay封装的音频解码器，并将解码器上下文avctx和Decoder绑定
         auddec.decoder_init(avctx,&audioq);
         //启动音频解码线程
         if(!audio_change_source)
         auddec.decoder_start(AVMEDIA_TYPE_AUDIO,"audio_thread",this);
         //允许音频输出,再此打开音频输出依旧可以进行播放音频
         //play audio
        /*222222222222222222222222222*/
         SDL_PauseAudio(0);
         seek_rel =sample_rate * 1024;
         is_audio = 1;
//         audio_change_source = 0;
         break;
        case AVMEDIA_TYPE_VIDEO:
             video_stream = stream_index;//等于2
             video_st = ic->streams[stream_index];
             //初始化ffplay封装的视屏解码器
             viddec.decoder_init(avctx,&videoq);
             //启动音频解码线程
         if(!video_change_source)
         viddec.decoder_start(AVMEDIA_TYPE_VIDEO,"video_thread",this);
         is_video = 1;
//         audio_change_source = 0;



         break;
     default:
         break;
     }
     goto out;
fail:
     avcodec_free_context(&avctx);
out:
     return ret;

}
Decoder::Decoder()//Decoder对象构造函数
{
    av_init_packet(&pkt_);
}
Decoder::~Decoder()//Decoder对象析构函数
{

}

int FFPlayer::ffp_start_l()
{
    //触发播放
//    std::cout << __FUNCTION__<<"111111111111111111";
//        qDebug() << "hello 666666666666666666666611111111111111111111111111!";
    pause_ = false;
}

int FFPlayer::ffp_stop_l()
{
    abort_request = 1;//请求退出
    msg_queue_abort(&msg_queue_);//禁止再插入消息，队列请求退出
}
//关闭解复用器线程
int FFPlayer::stream_component_close(int stream_index)
{
    AVCodecParameters *codecpar;//描述音视频编码参数
    if(stream_index < 0 || stream_index >= ic->nb_streams){//流出现错误了
       return -1;
    }
    codecpar = ic->streams[stream_index]->codecpar;
    switch (codecpar->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        std::cout<< __FUNCTION__ << "AVMEDIA_TYPE_AUDIO\n";
        //请求终止解码器线程
//        if(video_thread&&video_refresh_thread_->joinable()){
//            video_refresh_thread_->joina();//等待线程退出
//        }
        /*decoder_abort:
         * 1.进行回收解码线程joinable
         * 2.delete 线程分配内存
         * 3.将线程置为NULL
          */
//        auddec.decoder_abort(&sampq);
        //关闭音频设备
        audio_close();

        //销毁解码器
        auddec.decoder_destroy();
        //释放重采样器
        swr_free(&swr_ctx);
        //释放audio_buff
        av_free(&audio_buf1);
        audio_buf1_size = 0;
        audio_buf = NULL;
        break;
    case AVMEDIA_TYPE_VIDEO:
        //请求退出视频画面刷新线程
//        if(decode_thread_&&decode_thread_->joinable()){
//            decode_thread_->joinable();//等待线程退出
//        }
        std::cout<<__FUNCTION__<<"AVMEDIA_TYPE_AUDIO\n";
        //请求终止解码器线程
        //关闭视频设备
        //销毁解码器
//        viddec.decoder_abort(&pictq);
        viddec.decoder_destroy();

        break;
    default:
        break;
    }
    switch (codecpar->codec_type) { //解码器参数类型
    case AVMEDIA_TYPE_AUDIO:
        audio_st = NULL;
        audio_stream = -1;
        break;
    case AVMEDIA_TYPE_VIDEO:
        video_st = NULL;
        video_stream = -1;
        break;
    default:
        break;
    }
}
void FFPlayer::audio_close()
{
    /*44444444444444444444444*/
    SDL_CloseAudio();//SDL_CloseAudioDevice
    SDL_Quit();
}
int FFPlayer::read_thread()
{

    int err,i,ret;
    int st_index[AVMEDIA_TYPE_NB];//AVMEDIA_TYPE_VIDEO/AVMEDIA_TYPE_AUDIO等，用来保存stream_index,流索引
    AVPacket pkt1;
    AVPacket *pkt = &pkt1;
    char protocol_buf[5] = {0};
    const char* ddl = NULL;


    //初始化为-1,如果一直为-1,说明没响应stream
    memset(st_index,-1,sizeof(st_index));
    video_stream = -1;
    audio_stream = -1;
    eof = 0;

    //1.创建上下文结构体，这个结构体是最上层的结构体，表示输入上下文
    ic = avformat_alloc_context();
    if(!ic){
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    //2.打开文件，主要是探测协议类型，如果是网络文件则创建网络链接等
    err = avformat_open_input(&ic,input_filename_,NULL,NULL);
    if(err<0){
        ret = -1;
        goto fail;
    }

    ddl = input_filename_;
    strncpy(protocol_buf,ddl, 4);
    if((ret = strcmp(protocol_buf,"rtmp")) == 0)
          is_realtime = 1;
    printf("111111111111111%d\n",ret);

    //再此由ffplayer发送FFP_MSG_OPEN_INPUT
    ffp_notify_msg1(this, FFP_MSG_OPEN_INPUT);//这个this包含了创建线程的ffplayer对象属性
    std::cout<<"read_thread FFP_MSG_OPEN_INPUT " << this << std::endl;
    //3.进一步解析stream流中各参数信息
    err = avformat_find_stream_info(ic,NULL);
    if(err<0){
        ret = -1;
        goto fail;
    }
    ffp_notify_msg1(this, FFP_MSG_FIND_STREAM_INFO);
    std::cout<<"read_thread FFP_MSG_FIND_STREAM_INFO " << this << std::endl;
    //4.利用av_find_best_stream选择查找相应流索引
    st_index[AVMEDIA_TYPE_VIDEO] = av_find_best_stream(ic,AVMEDIA_TYPE_VIDEO,st_index[AVMEDIA_TYPE_VIDEO],-1,NULL,0);
    st_index[AVMEDIA_TYPE_AUDIO] = av_find_best_stream(ic,AVMEDIA_TYPE_AUDIO,st_index[AVMEDIA_TYPE_AUDIO],st_index[AVMEDIA_TYPE_VIDEO],NULL,0);

    //open the stream
    //5.打开视频，音频解码器，在此打开相应的解码器，并创建相应的解码线程
    if(st_index[AVMEDIA_TYPE_AUDIO] >= 0){ //如果有音频流则打开音频流
     ret =   stream_component_open(st_index[AVMEDIA_TYPE_AUDIO]);
    }

    if(st_index[AVMEDIA_TYPE_VIDEO] >= 0 && strcmp(input_filename_,"music.flac")!=0 && strcmp(input_filename_,"Samurai.mp3")!=0){ //如果有视频流则打开视频流
      ret = stream_component_open(st_index[AVMEDIA_TYPE_VIDEO]);

    }
        ffp_notify_msg1(this,FFP_MSG_COMPONENT_OPEN);
        std::cout << "read_thread FFP_MSG_COMPONENT_OPEN " << this << std::endl;
    if(audio_stream < 0 && video_stream < 0){
            ret  = -1;
            goto fail;
        }
        wait_mutex = SDL_CreateMutex();
        wait_read_thread = SDL_CreateCond();
            ffp_notify_msg1(this, FFP_MSG_PREPARED);
            std::cout << "read_thread FFP_MSG_PREPARED " << this << std::endl;

//            packet_queue_flush(&videoq);
//            packet_queue_flush(&audioq);
//            //并向队列插入flushpkt
//            //填充空包
//            auddec.flush_pkt = {0};
//            viddec.flush_pkt = {0};
//            //不用分配时序，当队列收到空包之后自然就开始进行队列时序自加了
//            //将空包插入响应的包队列
//            packet_queue_put(&videoq,&viddec.flush_pkt);//再进行放空包清除frame_queue
//            packet_queue_put(&audioq,&auddec.flush_pkt);



//    ffp_notify_msg1(this, FFP_MSG_OPEN_INPUT);
//    std::cout<<"read_thread FFP_MSG_OPEN_INPUT " << this << std::endl;
//    ffp_notify_msg1(this, FFP_MSG_FIND_STREAM_INFO);
//    std::cout<<"read_thread FFP_MSG_FIND_STREAM_INFO " << this << std::endl;
//    ffp_notify_msg1(this,FFP_MSG_COMPONENT_OPEN);
//    std::cout << "read_thread FFP_MSG_COMPONENT_OPEN " << this << std::endl;
//    ffp_notify_msg1(this, FFP_MSG_PREPARED);
//    std::cout << "read_thread FFP_MSG_PREPARED " << this << std::endl;

while (1) {

//    if(change_source)
//    {
//        err = avformat_open_input(&ic,input_filename_,NULL,NULL);
//        if(err<0){
//            ret = -1;
//            goto fail;
//        }
//        err = avformat_find_stream_info(ic,NULL);
//        st_index[AVMEDIA_TYPE_VIDEO] = av_find_best_stream(ic,AVMEDIA_TYPE_VIDEO,st_index[AVMEDIA_TYPE_VIDEO],-1,NULL,0);
//        st_index[AVMEDIA_TYPE_AUDIO] = av_find_best_stream(ic,AVMEDIA_TYPE_AUDIO,st_index[AVMEDIA_TYPE_AUDIO],st_index[AVMEDIA_TYPE_VIDEO],NULL,0);
//        change_source =0;
//        audio_change_source =1;
//        video_change_source =1;
//        if(st_index[AVMEDIA_TYPE_AUDIO] >= 0){ //如果有音频流则打开音频流
//         ret =   stream_component_open(st_index[AVMEDIA_TYPE_AUDIO]);
//        }

//        if(st_index[AVMEDIA_TYPE_VIDEO] >= 0 && strcmp(input_filename_,"music.flac")!=0 && strcmp(input_filename_,"Samurai.mp3")!=0){ //如果有视频流则打开视频流
//         ret =   stream_component_open(st_index[AVMEDIA_TYPE_VIDEO]);

//        }

//    }

//    std::cout<<"read_thread sleep, mp:" << this << std::endl;//先模拟线程运行


//    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
          if(abort_request)//首先read_thread可以正常退出
    {
            break;//这一break就会让这个线程重新进行一遍stream_open，或许由于重复创建线程导致播放器卡死
    }
          if(seek_req)//因为在请求seek的过程中我随眠了100ms，所以会刷新几帧
          {

             //1.全局serial++
              serial_ ++;
             //2.初始化队列
             packet_queue_flush(&videoq);
             packet_queue_flush(&audioq);
             //并向队列插入flushpkt
             //填充空包
             auddec.flush_pkt = {0};
             viddec.flush_pkt = {0};
             //不用分配队列时序，当队列收到空包之后自然就开始进行队列时序自加了
             //将空包插入响应的包队列
             packet_queue_put(&videoq,&viddec.flush_pkt);//再进行放空包清除frame_queue
             packet_queue_put(&audioq,&auddec.flush_pkt);
             //3.执行seek
             avformat_seek_file(ic,st_index[AVMEDIA_TYPE_AUDIO],min_ts,ts,max_ts,seek_flags);
//             frame_queue_flush(&pictq);
//             frame_queue_flush(&sampq);


             seek_req = 0;
             eof = 0;
             seek_single_step = 1;
//             setClock(&auddic,0);//将这一帧音频pts设置为auddec的时间参照


          //1.   清空解码器内部的帧缓存（包括未解码完的 Packet、未输出的 Frame）；
          //2.   重置解码器的状态（如参考帧列表、时间戳计数器）；
          //3.   让解码器回到「初始就绪状态」，等待新的输入数据。
          //4.   说明解码器上下文是实时记录着解码的情况


          }

          //读取媒体数据，得到的是音视频分离，解码前的数据
         //其实read_thread是一直在读包，
//           std::cout<<"99999999999999999999:"<<pause_<<std::endl;
    //      std::cout<<"99999999999999999999:"<<ts<<std::endl;
//          if(!pause_){
//           std::cout<<"-------------:"<<pause_<<std::endl;

         //在暂停时会在此等待
          if(stream_has_enough_packets(audio_st,st_index[AVMEDIA_TYPE_AUDIO],&audioq) ||
                  stream_has_enough_packets(video_st,st_index[AVMEDIA_TYPE_VIDEO],&videoq)){

              SDL_LockMutex(wait_mutex);
              SDL_CondWaitTimeout(wait_read_thread,wait_mutex,10);
              SDL_UnlockMutex(wait_mutex);
              continue;
          }


          ret = av_read_frame(ic,pkt);//调用不会释放pkt的数据，需要我们自己去释放packet的数据,以相应的音频上下文和视频上下文来读取pkt包
          if(ret < 0){ //出错或者已经读取完毕了
              if((ret == AVERROR_EOF || avio_feof(ic->pb))&&!eof)//pb 就是 AVFormatContext 用来 “读数据”（如从文件读视频流）或 “写数据”（如向文件写编码后的数据）的 “工具”，隐藏了不同 IO 场景（本地文件、HTTP 流、内存缓冲区等）的底层实现差异。
              eof = 1; //已达到数据流末端，数据读完

          if(ic->pb && ic->pb->error)
              break;
          std::this_thread::sleep_for(std::chrono::milliseconds(10)); //读完数据后这里进行休眠
          continue; //继续循环
    }else{
      eof = 0;
    }

    //插入队列，先只处理音频包
    if(pkt->stream_index == audio_stream){
        packet_queue_put(&audioq,pkt);
    }else if(pkt->stream_index == video_stream){
       packet_queue_put(&videoq,pkt);
    }else{
       av_packet_unref(pkt);//不入队列则直接释放数据
    }
//        }

//       else std::this_thread::sleep_for(std::chrono::milliseconds(10)); //读完数据后这里进行休眠毫秒

}

      std::cout<< __FUNCTION__ << "leave" <<std::endl;
  return 0;
      fail:
        return -1;
        stream_close();
}

#define REFRESH_RATE 0.01
int FFPlayer::video_refresh_thread()
{
    double remaining_time = 0.0;

    while (!abort_request) {//当置为1之后请求退出跳出循环
        if(remaining_time > 0.0)
            av_usleep((int)(remaining_time * 1000000.0));
         remaining_time = REFRESH_RATE;
//        if(is_audio == 0)
//        {
//           remaining_time = REFRESH_RATE*4;
//        }
        if(!pause_||step == 1 || pause_ && seek_single_step == 1)
        video_refresh(&remaining_time);


    }
    std::cout<<__FUNCTION__<<"leave"<<std::endl;
}

double FFPlayer::video_refresh(double *remaining_time)//这个视频播放应该先
{
     Frame *vp = NULL;//封装的frame帧数据
     static int last_serial = serial_;//初始化static赋值
     static int ser_fresh_count = 0;
    //目前我们先是只有队列中的视频帧可以放，就播出来
    //判断有没有视频画面
    seek_single_step = 0;
    step = 0;//将step放在这里几乎立刻变为0,步进的精确性防止音频再次刷新
    if(video_st)
    {
        if(frame_queue_nb_remaining(&pictq) == 0)//队列中没有数据了
        //什么都不用做，可以退出了
        return 0;
    }

//    if(viddec.pkt_serial_ != serial_){//当时序相等时
//        for(int i = 0;i < 2; i++){
//            vp = frame_queue_peek(&pictq); //读取待显示帧
//            std::this_thread::sleep_for(std::chrono::milliseconds(20));
//            if(video_refresh_callback_){
//                video_refresh_callback_(vp);
//        }
//       }
//        serial_ = viddec.pkt_serial_;
//    }
    //能跑到这里已经说明队列不为空，肯定有frame可以读取
    vp = frame_queue_peek(&pictq); //读取待显示帧




//            frame_queue_next(&pictq);//当前vp帧出列
//      std::this_thread::sleep_for(std::chrono::milliseconds(20));



//    }


//  if(last_serial == serial_){
    if (vp->serial != videoq.serial) {
        frame_queue_next(&pictq);

    }
    //如果没有音频直接跳出等待
    if(is_audio){
      audio_clock = get_clock(&auddic);//获取音频时钟

    if(vp->pts > audio_clock)//这个是在同一时序下进行比较的
     {
        *remaining_time = FFMIN(vp->pts - audio_clock,*remaining_time);
//        if( *remaining_time < 0)
        return *remaining_time;
     }

    }
    else{
        return *remaining_time;
    }

//      }
//   }
//    else
//    {
//         ser_fresh_count ++;
//         //这里是为了给解码器留时间解码
//         std::this_thread::sleep_for(std::chrono::milliseconds(40));
//    }



    std::cout<<"vp->pts:"<<vp->pts;
    //刷新显示
    if(video_refresh_callback_){
        //在渲染之前进行数据引用拷贝计数，
        // std::shared_ptr<const Frame*> OpenGL_vp = std::make_shared<const Frame*>(vp);//vp引用计数为1
        video_refresh_callback_(vp);
        screenshot(vp->frame);
        frame_queue_next(&pictq);//当前vp帧出列

        //当不同时序时，刷到下一帧自动会更新时序，主要是那一帧卡住了时间戳对比音频时钟
        //我先让视频快5帧然后等待音频，只要是音频开始维护音频时钟之后，我再利用视频进行跟随音频进行播放，这样视频就能不论哪里都能进行seek播放
            //之前那种不行是因为，往后退时，那很大的一帧视频帧把vp->pts给固定住了，所以我用这个方法先无论seek到哪里先把那一帧给放出去再进行跟随音频播放

//        if(speed!=2){
//        if(ser_fresh_count == 4)
//        {

//          last_serial = serial_;
//          ser_fresh_count = 0;
//          seek_single_step = 0;

//          //这个是为了下次seek做时间间隔
////       std::this_thread::sleep_for(std::chrono::milliseconds(100));

//        }
    }
//        else if(ser_fresh_count == 9)
//            {

//              last_serial = serial_;
//              ser_fresh_count = 0;
//              seek_single_step = 0;
//            }
//        }

//    else
//        std::cout<<__FUNCTION__<<"video_fresh_callback_ NULL"<<std::endl;

  }
//        viddec.pkt_serial_ = serial_;
// }
//}
void FFPlayer::AddVideoRefreshCallback(
         std::function<int(const Frame *)>callback)
{
    video_refresh_callback_ = callback;//通过调用这个函数进行回调
}



void Decoder::decoder_init(AVCodecContext *avctx,PacketQueue *queue) {


//    memset(d, 0, sizeof(Decoder));
    avctx_ = avctx;
    queue_ = queue;
//    d->empty_queue_cond = empty_queue_cond;
//    d->start_pts = AV_NOPTS_VALUE;
//    d->pkt_serial = -1;
}

int Decoder::decoder_start(enum AVMediaType codec_type,const char *thread_name, void* arg)
{

    packet_queue_start(queue_);
//    d->decoder_tid = SDL_CreateThread(fn, thread_name, arg);
    if (AVMEDIA_TYPE_VIDEO == codec_type) {
       decode_thread_ = new std::thread(&Decoder::video_thread,this,arg);
    }
    else if(AVMEDIA_TYPE_AUDIO == codec_type)
    {
        decode_thread_ = new std::thread(&Decoder::audio_thread,this,arg);
    }
    else
        return -1;
    return 0;
}
void Decoder::decoder_abort(FrameQueue *fq)
{
//    packet_queue_abort(queue_);//请求退出包队列
//    frame_queue_signal(fq);  //唤醒阻塞的帧队列

//    if(decode_thread_ && decode_thread_->joinable()){
//        decode_thread_->join();//解码线程回收
//       // join主线程永久阻塞，直到子线程终止	detach主线程无阻塞，调用后立刻返回
////        delete decode_thread_; //删除解码线程内存
////        decode_thread_ = NULL; //解码线程句柄置为NULL
//    }
//    else  if(audio_decode_thread_ && audio_decode_thread_->joinable()){
//        audio_decode_thread_->join();//解码线程回收
//        delete audio_decode_thread_; //删除解码线程内存
//        audio_decode_thread_ = NULL; //解码线程句柄置为NULL
//    }
//    packet_queue_flush(queue_); //清除packet队列，并释放数据
//    SDL_WaitThread(d->decoder_tid, NULL);
//    d->decoder_tid = NULL;
//    packet_queue_flush(d->queue);
}

void Decoder::decoder_destroy() {
    av_packet_unref(&pkt_); //释放包
    avcodec_free_context(&avctx_);//释放解码器上下文
}
int Decoder::decoder_decode_frame(AVFrame *frame) {
 int ret = AVERROR(EAGAIN); //是为空
 //video_thread解码线程会一直调用decoder_decode_frame
 for (;;) {
        AVPacket pkt;//中间包

           if(queue_->serial == pkt_serial_){//
            do { //第一个循环先把codec里的frame全部读完
                if (queue_->abort_request) //decoder_abort调用的时候触发queue->abort_request为1
                    return -1; //请求退出

                switch (avctx_->codec_type) { //根据解码器类型，选择相应的解码器进行解码
                    case AVMEDIA_TYPE_VIDEO://一般情况下这个dowhile会执行两次
                        ret = avcodec_receive_frame(avctx_, frame);//这个dowhile循环是进行尽力解码，解完就退出，没有可解码的
                        //pkt就去从pktqueue队列中获取一个pkt，并且发送到ffmpeg解码器进行解码，解码完成退出返回1
                      //解码后的视频时间戳则进行算法计算与音频进行同步，这种架构是紧于进行解码pkt进行获取包，由于没有缓冲队列紧于解码算法
                        //printf("frame pts:%ld, dts:%ld\n", frame->pts, frame->pkt_dts);
                        if (ret >= 0) {

//                            if (decoder_reorder_pts == -1) {
                                frame->pts = frame->best_effort_timestamp;
//                            } else if (!decoder_reorder_pts) {
//                              } else  {
//                        frame->pts = frame->pkt_dts;

//                            }
                        }//在拿到frame数据之后进行
//                        else {
//                            char errStr[256] = {0};
//                            printf("video dec:%s\n",errStr);
//                        }
                        break;//如果ret大于0说明收到解码后的frame帧，然后break退出
                    case AVMEDIA_TYPE_AUDIO:
                        ret = avcodec_receive_frame(avctx_, frame);//获取一帧音频帧
                        //当avcodec还没有获得pkt包进行解码的时候，返回AVERROR（EAGAIN)
                        if (ret >= 0) {
                            AVRational tb = (AVRational){1, frame->sample_rate};
                            if (frame->pts != AV_NOPTS_VALUE){
                                //如果frame->pts帧的时间戳正常的情况下，现将pkt_timebase转成{1, frame->sample_rate}
                                //pkt_timebase实质上就是stream->time_base
                             //是在解码后进行了帧时间戳与流的时基timebase转换
                                frame->pts = av_rescale_q(frame->pts, avctx_->pkt_timebase, tb);
                             }
//                            else if (d->next_pts != AV_NOPTS_VALUE)

//                                frame->pts = av_rescale_q(d->next_pts, d->next_pts_tb, tb);
//                            if (frame->pts != AV_NOPTS_VALUE) {
//                                d->next_pts = frame->pts + frame->nb_samples;
//                                d->next_pts_tb = tb;
//                            }
                        }
//                        else {
//                            char errStr[256] = { 0 };
//                            printf("audio dec:%s\n",errStr);
//                        }
                        break;
                }
                //1.3检查解码器是否已经结束，解码结束返回0
          //如果出现解码错误,就重新解码器上下文
                if (ret == AVERROR_EOF) {
//                    finished_ = pkt_serial_;
                    printf("avcodec_flush_buffers %s(%d)\n", __FUNCTION__, __LINE__);
                    avcodec_flush_buffers(avctx_);//如果收到错误，那就将解码缓冲区清空
                     return 0;
                }
                //正常解码返回1，正常情况下就相当于return 1，这个while循环没啥用
                 if (ret >= 0)
                     return 1;//其实在获取到一帧frame就跳出线程，return是跳出整个函数
                 //如果解码专属队列不为空，就进行do循环，真正读取一帧就退出while循环
            } while (ret != AVERROR(EAGAIN));
           }//主要就是在
           // 2 获取一个packet，如果播放序列不一致(数据不连续)则过滤掉“过时”的packet
//           do {
//               // 2.1 如果没有数据可读则唤醒read_thread, 实际是continue_read_thread SDL_cond
//               //            if (queue_->nb_packets == 0)  // 没有数据可读
//               //                SDL_CondSignal(empty_queue_cond);// 通知read_thread放入packet
//               // 2.2 如果还有pending的packet则使用它
////               if (packet_pending_) {
////                   av_packet_move_ref(&pkt, &pkt_);
////                   packet_pending_ = 0;
////               } else {
//                   // 2.3 阻塞式读取packet，从队列中获取serial
//                   if (packet_queue_get(queue_, &pkt, 1, &pkt_serial_) < 0) {
//                       return -1;
//                   }
////               }
//               if(queue_->serial != pkt_serial_) {
//                   // darren自己的代码
//                   qDebug() << "discontinue:queue->serial:" << queue_->serial << ", pkt_serial:" << pkt_serial_;
//                   av_packet_unref(&pkt); // fixed me? 释放要过滤的packet
//               }
//           } while (queue_->serial != pkt_serial_);// 如果不是同一播放序列(流不连续)则继续读取
           if (pkt.data == flush_pkt.data) {//
               // when seeking or when switching to a different stream
               avcodec_flush_buffers(avctx_); //清空里面的缓存帧
               finished_ = 0;        // 重置为0
               seek_for_set_audio_clock_ = 1;
               next_pts = start_pts;    // 主要用在了audio
              // next_pts_tb = start_pts_tb;// 主要用在了audio
           }
        //在第一调用avcodec_receive_frame时返回的就是AVERROR(EAGAIN)退出循环
        //没帧可读时，ret返回EAGIN，需要继续送packet
        //在目前这个版本我们不去检查播放序列问题
        //如果上面的循环获取到了frame这里不会被执行，第二个循环，主要是读取packet送给解码器
        //阻塞式读取packet，我们顶峰相见！！！
        //在packetqueueget中读取packet包,通过pkt_queue获取pkt包并放到pkt包中
        if(packet_queue_get(queue_,&pkt,1,&pkt_serial_)<0)//从packet队列中获取一个pkt并且赋予serial,此时decoder已经有最新serial
            return -1;  //是在刚刷新空包时就serial++,所以最新包也是自带最新serial
//        if (pkt.data == flush_pkt.data) {//如果是空包,那不得清除frame_queue
//            pkt.data = NULL;
//            pkt.size = 0;
//        }
       //这个是将packet包发送到解码器进行解包，发送这个pkt刚刚从pktqueue队列中获取的包到avcodec解码器中，这是ffmpeg中的API（根据这个avctx解码器上下文进行解包）
        if(avcodec_send_packet(avctx_,&pkt) == AVERROR(EAGAIN)){
            // AVERROR(EAGAIN)，出现这个问题不能发送到解码器里面去
//            return -1;

        }
        //发送完之后就可以释放包数据了
        av_packet_unref(&pkt);//一定要去释放音视频数据
        //以上循环全部都for循环里进行不断解码出frame帧数据
       }
    }







//        do {
//            if (d->queue->nb_packets == 0)  // 没有数据可读
//                SDL_CondSignal(d->empty_queue_cond);

//            if (d->packet_pending) {
//                av_packet_move_ref(&pkt, &d->pkt);
//                d->packet_pending = 0;
//            } else {
//                if (packet_queue_get(d->queue, &pkt, 1, &d->pkt_serial) < 0)
//                    return -1;
//            }
//        } while (d->queue->serial != d->pkt_serial);

//        if (pkt.data == flush_pkt.data) {
//            // when seeking or when switching to a different stream
//            avcodec_flush_buffers(d->avctx); //清空里面的缓存帧
//            d->finished = 0;
//            d->next_pts = d->start_pts;
//            d->next_pts_tb = d->start_pts_tb;
//        } else {
//            if (d->avctx->codec_type == AVMEDIA_TYPE_SUBTITLE) {
//                int got_frame = 0;
//                ret = avcodec_decode_subtitle2(d->avctx, sub, &got_frame, &pkt);
//                if (ret < 0) {
//                    ret = AVERROR(EAGAIN);
//                } else {
//                    if (got_frame && !pkt.data) {
//                       d->packet_pending = 1;
//                       av_packet_move_ref(&d->pkt, &pkt);
//                    }
//                    ret = got_frame ? 0 : (pkt.data ? AVERROR(EAGAIN) : AVERROR_EOF);
//                }
//            } else {
//                if (avcodec_send_packet(d->avctx, &pkt) == AVERROR(EAGAIN)) {
//                    av_log(d->avctx, AV_LOG_ERROR, "Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
//                    d->packet_pending = 1;
//                    av_packet_move_ref(&d->pkt, &pkt);
//                }
//            }
//            av_packet_unref(&pkt);	// 一定要自己去释放音视频数据
//        }




int Decoder::get_video_frame(AVFrame *frame)
{
        int got_picture;
    //利用紧于解码算法强调处理每一个pkt（不能错过）;
//         got_picture =  decoder_decode_frame(frame);
//         printf("%d\n",got_picture);
    if ((got_picture = decoder_decode_frame(frame)) < 0)//成功解出来数据之后
        return -1;
//     if(got_picture >= 0)
//       got_picture = 1;


    if (got_picture) {
//        double dpts = NAN;
     //解析获取到的该帧是否要drop掉，该机制的目的是在放入帧队列前先drop掉过时的视频帧

    }
//        if (frame->pts != AV_NOPTS_VALUE)
//            dpts = av_q2d(is->video_st->time_base) * frame->pts;

//        frame->sample_aspect_ratio = av_guess_sample_aspect_ratio(is->ic, is->video_st, frame);

//        if (framedrop>0 || (framedrop && get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER)) {
//            if (frame->pts != AV_NOPTS_VALUE) {
//                double diff = dpts - get_master_clock(is);
//                if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD &&
//                    diff - is->frame_last_filter_delay < 0 &&
//                    is->viddec.pkt_serial == is->vidclk.serial &&
//                    is->videoq.nb_packets) {
//                    is->frame_drops_early++;
//                    av_frame_unref(frame);
//                    got_picture = 0;
//                }
//            }
//        }
//    }

   return got_picture;//return1;

}
int Decoder::queue_picture(FrameQueue *fq,AVFrame *src_frame,double pts,//这个picq队列会在成员满时进行无限等待
                  double duration,int64_t pos, int serial)
{

    Frame *vp;


    if (!(vp = frame_queue_peek_writable(fq))) // 检测队列是否有可写空间，将写入指针放到可写入wri_index索引
        return -1;      // Frame队列满了则返回-1
    // 执行到这步说已经获取到了可写入的Frame
//    vp->sar = src_frame->sample_aspect_ratio;
//    vp->uploaded = 0;

    vp->width = src_frame->width;
    vp->height = src_frame->height;
    vp->format = src_frame->format;

    vp->pts = pts;
    vp->duration = duration;
//    vp->pos = pos;
    vp->serial = serial;

//    set_default_window_size(vp->width, vp->height, vp->sar);
  //将传入来的这一帧src_frame数据进行内存数据填充
    av_frame_move_ref(vp->frame, src_frame); // 将src中所有数据拷贝到dst中，并复位src。
    //将这一帧数据放到frame_queue队列之中
    frame_queue_push(fq);   // 更新写索引位置
    return 0;
}
//线程已创建在stream_compoent_open中
//音频通过audio_thread进行音频解码而音频播放通过sdl_回调函数进行自给自足的sdl音频播放
int Decoder::audio_thread(void *arg)//这个类调用这个decoder_decode_frame
{
    std::cout << __FUNCTION__ << "into"  <<std::endl;
    FFPlayer *is = (FFPlayer*)arg;//使用FFPlay类进行创建audio_thread线程，同时传入FFPlayer参数进行参数访问以及转换
    AVFrame *frame = av_frame_alloc();//分配解码帧，这个仅仅是结构体内存，帧内内存还没有分配
    Frame *af;

    int got_frame = 0;//是否读取到帧
    AVRational tb; //timebase，有理数格式
    int ret = 0;
    //如果没有一帧数据，AVERROR(ENOMEM)，内存不足
    if (!frame)
        return AVERROR(ENOMEM);

    do {
        if(is->abort_request)
        {
            break;
        }
       if(seek_for_set_audio_clock_)
       {
          is->seek_for_set_audio_clock = 1;
          seek_for_set_audio_clock_ = 0;
       }
        start_pts = is->ts;
//        serial_ = is->serial_ ;
        //1.读到解码帧
        //返回int型，解完一帧数据就返回1，
        if ((got_frame = decoder_decode_frame(frame)) < 0)//这个解码器至死至终都在一直进行解码，在seek冲刷的一瞬间
            //放入的是flush_pkt，而此时的解码器解码到flush_pkt之时，我的sdl_audio_callback和video_refresh一瞬间相当于静止用的还是
            //上一个的serial，那时的flush_pkt也带着最新的序列，空包解码也带着最新的序列，所以会有空白帧出现
            //那么再来什么时候
            goto the_end;
//         if(got_frame >= 0)
//             got_frame = 1;

        if (got_frame) {
            //没有重转换时间戳而是
                tb = (AVRational){1, frame->sample_rate}; //设置sample_rate 为timebase
                /*下面全是对解码后frame帧的处理:
                 *1.处理pts帧时间戳然后转化成秒
                 *2.单帧时长duration
                 *然后再插入framequeue队列
                 */
             //获取可写入frame
                if (!(af = frame_queue_peek_writable(&is->sampq)))//获取可写帧
                    goto the_end;
                //设置Frame并放入FrameQueue
                af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts* av_q2d(tb);//转换时间戳
              //  af->pos = frame->pkt_pos;
              //  af->serial = is->auddec.pkt_serial;
                af->duration = av_q2d((AVRational){frame->nb_samples, frame->sample_rate});//获取每帧时长

                av_frame_move_ref(af->frame, frame);
                frame_queue_push(&is->sampq);


        }
    } while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);//如果满足则继续循环，不满足则跳出失败
 the_end:
    std::cout<< __FUNCTION__ << "leave" << std::endl;
    av_frame_free(&frame);//只要把frame中的核心数据给传递走了之后，相当于进行释放frame数据
    return ret;
}
//这个video_thread以及audio_thread都是第一解码线程(第一个调用的函数)
//视频解码线程用video_thread
//视频播放线程在video_refresh_thread线程(在stream_open时就和同时创建)进行视频刷新视频
int Decoder::video_thread(void *arg)
{
    std::cout << __FUNCTION__ << "into" << std::endl;
    FFPlayer *is = (FFPlayer*)arg;
    AVFrame *frame = av_frame_alloc(); //分配解码帧
    double pts;    //pts
    double duration; //帧持续时间
    int ret;
    //获取stream timebase
    AVRational tb = is->video_st->time_base; // 获取stream timebase
   // 获取帧率，以便计算每帧picture 的duration
    AVRational frame_rate = av_guess_frame_rate(is->ic, is->video_st, NULL);

    if (!frame)
        return AVERROR(ENOMEM);
 //video_thread线程的精髓就在这个for循环
    for (;;) {  // 循环取出视频解码的帧数据

//        if(pause_d)
//        {
//            std::this_thread::sleep_for(std::chrono::milliseconds(10)); //读完数据后这里进行休眠
//        }
        if(is->abort_request)
        {
            break;
        }


        ret = get_video_frame(frame);//调用这个函数时自能解码出一帧frame数据，并且返回1
        if (ret < 0)//均遵循 “0 = 成功、负数 = 错误、正数 = 特殊状态
            goto the_end; //解码结束，什么时候会结束
//       std::cout<<"ret======"<<ret;
        if (!ret)           // 没有解码得到画面，什么情况下会得不到解码后的帧
            continue;//如果没有退出，则继续获得frame解码帧

    /*下面全是对解码后frame帧的处理:
     *1.处理pts帧时间戳然后转化成秒
     *2.单帧时长duration
     *  然后再插入framequeue队列
     */

        //4.计算帧持续时间和换算pts值为秒
            // 1/帧率 = duration 单位秒, 没有帧率时则设置为0，有帧率则计算出帧间隔(通过帧率)
            duration = (frame_rate.num && frame_rate.den ? av_q2d((AVRational){frame_rate.den, frame_rate.num}) : 0);
            //根据AVStream timebase计算出pts值，单位为秒
            pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
            // 插入视频帧队列，将解码后的视频帧插入队列
            //这个queue_picture和音频中的frame_queue帧插入队列本质上是一样的，这里函数调用格式显得有点不一样
            ret = queue_picture(&is->pictq, frame, pts, duration, frame->pkt_pos, is->viddec.pkt_serial_);
            //插入队列之后就进行释放frame对应的数据
            av_frame_unref(frame);


        if (ret < 0)
            goto the_end;
    }
 the_end:

    av_frame_free(&frame);
    return 0;
}
static int audio_decode_frame(FFPlayer *is)//对音频解码后的一帧音频数据进行按照sdl音频输出进行重采样
{
    int data_size, resampled_data_size;
    int64_t dec_channel_layout;
//    av_unused double audio_clock0;
    int wanted_nb_samples = 0;
    Frame *af;
    int ret = 0;

    while(is->pause_){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));//线程消息不能等太久10ms
        //？？？另外如果改为SDL_Cond_Wait,是否可以直接调用SDL_Cond_Singal进行条件唤醒呢？
        if(is->abort_request)
        {
            break;
        }
//        if(is->seek_single_step == 1){//只有seek触发时才会置为1,然后这个发挥作用必然在清空队列有数据的情况下发挥作用(这个待会可能会有问题)
//            is->step = 1;
//          is->seek_single_step = 0;
//        }
//    if(is->serial_ != is->auddec.pkt_serial_)//一直播放，直到视频那边成功播放跟随
//        {
//            is->step = 1;
//        }
        if(is->step == 1||is->seek_single_step == 1)//这个是为了
          break;
    }

//    if( is->serial_ == is->auddec.pkt_serial_){
//    do {
        if (!(af = frame_queue_peek_readable(&is->sampq)))//获取一帧可读samq音频数据
            return -1;
//        frame_queue_next(&is->sampq);
//    } while (af->serial != is->audioq.serial);
   //根据frame中指定的音频参数获取缓冲区的大小af->frame->channels *af->frame->nb_samples *2
   //一帧采样字节大小
    data_size = av_samples_get_buffer_size(NULL,
                                           af->frame->channels,
                                           af->frame->nb_samples,//样本数量
                                           (enum AVSampleFormat)af->frame->format, 1);//获取一帧音频帧的实际字节大小

    //获取声道布局，有就默认，没有就重新计算
    dec_channel_layout = //只有声道数channels，才是决定「音频字节大小」的核心,通道布局决定通道如何进行播放声音
        (af->frame->channel_layout && af->frame->channels == av_get_channel_layout_nb_channels(af->frame->channel_layout)) ?
        af->frame->channel_layout : av_get_default_channel_layout(af->frame->channels);
   //获取样本数校正值，若同步时钟是音频，则不调整样本数，否则根据同步需要调整样本

    wanted_nb_samples = af->frame->nb_samples;//采样个数，
    //为了2倍数我把wanted_nb_samples的大小给为原来的1/8同时也不知道为啥

    //这个audio_src是目标转化的结构体，也是
    if (af->frame->format        != is->audio_src.fmt            || //采样格式
        dec_channel_layout       != is->audio_src.channel_layout || //通道布局
        af->frame->sample_rate   != is->audio_src.freq           || //采样率
        (wanted_nb_samples       != af->frame->nb_samples && !is->swr_ctx)
           ) {
        //清空重采样上下文
        swr_free(&is->swr_ctx);

        //重采样上下文进行内存分配与参数供给
        is->swr_ctx = swr_alloc_set_opts(NULL,
                                         is->audio_tag.channel_layout, //目标输出
                                         is->audio_tag.fmt,
                                         is->audio_tag.freq,
                                         dec_channel_layout,//数据源
                                         (enum AVSampleFormat)af->frame->format,
//                                         af->frame->sample_aspect_ratio,
//                                         af->frame->format,
                                         af->frame->sample_rate,
                                         0, NULL);
        //参数赋予之后，进行初始化，这是重采样上下文有了参数
        if (!is->swr_ctx || swr_init(is->swr_ctx) < 0) {
//            av_log(NULL, AV_LOG_ERROR,
//                   "Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",
//                    af->frame->sample_rate, av_get_sample_fmt_name(af->frame->format), af->frame->channels,
//                    is->audio_tag.freq, av_get_sample_fmt_name(is->audio_tag.fmt), is->audio_tag.channels);
            swr_free(&is->swr_ctx);
            return -1;
            goto fail;
        }
        //源音频参数，解码前的原音频属性，注意这里的af已经是音频重采样后的数值，赋值给auio_src合情合理，即使注释掉了也能够正常运行
        is->audio_src.channel_layout = dec_channel_layout;
        is->audio_src.channels       = af->frame->channels;
        is->audio_src.freq = af->frame->sample_rate ;
        is->audio_src.fmt = (enum AVSampleFormat)af->frame->format;
    }

    if (is->swr_ctx) {
        //重采样输出参数
        const uint8_t **in = (const uint8_t **)af->frame->extended_data;


        uint8_t **out = &is->audio_buf1;//这样的话就是指向单个data[0]数组的具体成员
       //输出采样个数
        int out_count = (int64_t)wanted_nb_samples* is->audio_tag.freq/ af->frame->sample_rate + 256;
        // 4. 必加：采样数安全兜底（彻底解决爆音）
//        if (out_count < 256) out_count = 256;          // 防止采样数为0
//        if (out_count > af->frame->nb_samples * 2)     // 防止采样数突变过大
//            out_count = af->frame->nb_samples * 2;
       //输出采样字节大小
        int out_size  = av_samples_get_buffer_size(NULL, is->audio_tag.channels, out_count, is->audio_tag.fmt, 0);

        int len2;
        if (out_size < 0) {
//            av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() failed\n");
            return -1;
            goto fail;
        }

//        if (wanted_nb_samples != af->frame->nb_samples) {
//            if (swr_set_compensation(is->swr_ctx, (wanted_nb_samples - af->frame->nb_samples) * is->audio_tgt.freq / af->frame->sample_rate,
//                                        wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate) < 0) {
//                av_log(NULL, AV_LOG_ERROR, "swr_set_compensation() failed\n");
//                return -1;
//            }
//        }
        //if(audio_buf1 < out_size){重新分配out_size大小的缓存给audio_buf1,并将audio_buf1)_size设置为out_size}
        av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);//重分配大小内存
        if (!is->audio_buf1){
            return AVERROR(ENOMEM);
            goto fail;
        }
        //音频重采样，len2返回时重采样后得到的音频数据中单个声道的样本数
        len2 = swr_convert(is->swr_ctx, out, out_count, in, af->frame->nb_samples);//真正的重采样根据swr上下文进行转换数据在这里
        if (len2 < 0) {
//            av_log(NULL, AV_LOG_ERROR, "swr_convert() failed\n");
            return -1;
            goto fail;
        }

        if (len2 == out_count) {
//            av_log(NULL, AV_LOG_WARNING, "audio buffer is probably too small\n");
            if (swr_init(is->swr_ctx) < 0)//成功转化之后进行初始化swr_init
                swr_free(&is->swr_ctx);
        }
        //重采样返回的一帧音频数据大小(以字节为单位)
        is->audio_buf = is->audio_buf1;//将重采样后的地址给到audio_buf
        resampled_data_size = len2 * is->audio_tag.channels * av_get_bytes_per_sample(is->audio_tag.fmt);
    } else {
        //未经重采样，则将指针指向frame中的音频数据
        is->audio_buf = af->frame->data[0];//s16交错模式data[0],fltp_data[0] data[1]
        resampled_data_size = data_size;
    }
      if(is->seek_for_set_audio_clock)
      {
          is->setClock(&is->auddic,0);//重置音频时钟
          is->seek_for_set_audio_clock = 0;
      }
      //wanted_nb_samples采样数改变了音频速率，这个音频时间戳要乘以2
     else is->setClock(&is->auddic,af->pts*is->speed);//将这一帧音频pts设置为auddec的时间参照
      qDebug() << "seek_for_set_audio_clock:"<<is->ts*is->speed/1000000*20<<endl;

      is->is_audio =1;
      is->now_pts = af->pts*1000000;//精确到微妙
     std::cout<<"now->pts:"<<is->now_pts<<std::endl;
     frame_queue_next(&is->sampq);//才会真正释放frame    
     ret = resampled_data_size;//实际转换后的大小
//   }
//    else ret = 0;

//    audio_clock0 = is->audio_clock;
//    /* update the audio clock with the pts */
//    if (!isnan(af->pts))
//        is->audio_clock = af->pts + (double) af->frame->nb_samples / af->frame->sample_rate;
//    else
//        is->audio_clock = NAN;
//    is->audio_clock_serial = af->serial;

fail:
     return ret;
}
//这个回调函数进行自己自足地进行数据补给与消耗
static void sdl_audio_callback(void *opaque, Uint8 *stream, int len)
{
   FFPlayer *is = (FFPlayer*)opaque;
   int audio_size, len1;

  is->audio_callback_time_ = av_gettime_relative();
  //初始化
   SDL_memset(stream, 0, len);
   while (len > 0) {//循环读取，直到读取到足够的数据
       /* 如果is->audio_buf_index < is->audio_buf_size则说明上次拷贝还剩余一些数据，先拷贝到stream再调用audio_decode_frame */
       if (is->audio_buf_index >= is->audio_buf_size) {//当音频数据索引大于音频数据大小说明数据消耗完了
          audio_size = audio_decode_frame(is);//这个是音频进行重采样根据目标sdl的参数目标，也是要进行播放的一帧数据
          if (audio_size < 0) {
               /* if error, just output silence */
              is->audio_buf = NULL;
              is->audio_buf_size = SDL_AUDIO_MIN_BUFFER_SIZE / is->audio_tag.frame_size ;
              is->audio_no_data  = 1;      // 没有数据可以读取
                if(is->eof) {
                    // 如果文件以及读取完毕，此时应该判断是否还有数据可以读取，如果没有就该发送通知ui停止播放
                    is->check_play_finish();
                }
          } else {
//              if (is->show_mode != SHOW_MODE_VIDEO)
//                  update_sample_display(is, (int16_t *)is->audio_buf, audio_size);
              is->audio_buf_size = audio_size;//讲字节，多少字节
              is->audio_no_data = 0;
          }
          is->audio_buf_index = 0;
            // 2 是否需要做变速,先进行检测是否需要变速
            if(is->ffp_get_playback_rate_change()) {
                //已变速，标志位置0
                is->ffp_set_playback_rate_change(0);
                // 初始化,如果有变速上下文
                if(is->audio_speed_convert) {
                    // 则先释放
                    sonicDestroyStream(is->audio_speed_convert);
                }
                // 再重新创建一个变速上下文，通道数，采样率（采样格式用源数据的）
                is->audio_speed_convert = sonicCreateStream(is->get_target_frequency(),
                                          is->get_target_channels());
                // 设置变速系数，设置倍数进行变速播放
                sonicSetSpeed(is->audio_speed_convert, is->ffp_get_playback_rate());
                sonicSetPitch(is->audio_speed_convert, 1.0);
                sonicSetRate(is->audio_speed_convert, 1.0);
            }
            if(!is->is_normal_playback_rate() && is->audio_buf) {
                // 不是正常播放则需要修改
                // 需要修改  audio_buf_index audio_buf_size audio_buf
                int actual_out_samples = is->audio_buf_size /
                                         (is->audio_tag.channels * av_get_bytes_per_sample(is->audio_tag.fmt));
                // 计算处理后的点数
                int out_ret = 0;
                int out_size = 0;
                int num_samples = 0;
                int sonic_samples = 0;
                //格式正确，是sonic支持的格式
                if(is->audio_tag.fmt == AV_SAMPLE_FMT_FLT) {
                    //将audio_buf写入到sonic_stream流中
                    out_ret = sonicWriteFloatToStream(is->audio_speed_convert,
                                                      (float *)is->audio_buf,
                                                      actual_out_samples);
                } else  if(is->audio_tag.fmt == AV_SAMPLE_FMT_S16) {
                    out_ret = sonicWriteShortToStream(is->audio_speed_convert,
                                                      (short *)is->audio_buf,
                                                      actual_out_samples);
                } else {
                    av_log(NULL, AV_LOG_ERROR, "sonic unspport ......\n");
                }
                //sonic变速之后的采样个数
                num_samples =  sonicSamplesAvailable(is->audio_speed_convert);
                // 2通道  目前只支持2通道的
                out_size = (num_samples) * av_get_bytes_per_sample(is->audio_tag.fmt) * is->audio_tag.channels;
                av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);
                if(out_ret) {
                    // 从流中读取处理好的数据
                    if(is->audio_tag.fmt == AV_SAMPLE_FMT_FLT) {
                        //从sonic_stream中获取变速后的数据即audio_buf1
                        sonic_samples = sonicReadFloatFromStream(is->audio_speed_convert,
                                        (float *)is->audio_buf1,
                                        num_samples);
                    } else  if(is->audio_tag.fmt == AV_SAMPLE_FMT_S16) {
                        sonic_samples = sonicReadShortFromStream(is->audio_speed_convert,
                                        (short *)is->audio_buf1,
                                        num_samples);
                    } else {
                        qDebug()<< "sonic unspport fmt: " << is->audio_tag.fmt;
                    }
                    //数据指针转换，则变速成功
                    is->audio_buf = is->audio_buf1;
                    //                     LOG(INFO) << "mdy num_samples: " << num_samples;
                    //                     LOG(INFO) << "orig audio_buf_size: " << audio_buf_size;
                    is->audio_buf_size = sonic_samples * is->audio_tag.channels * av_get_bytes_per_sample(is->audio_tag.fmt);
                    //                    LOG(INFO) << "mdy audio_buf_size: " << audio_buf_size;
                    is->audio_buf_index = 0;
                }
            }
        }
        if(is->audio_buf_size == 0) {
            continue;
        }

       //根据缓冲区剩余大小量力而行
       len1 = is->audio_buf_size - is->audio_buf_index;
       if (len1 > len)
           len1 = len;
        //静音不播放数据，直接进行置0
       /* 判断是否为静音，以及当前音量的大小，如果音量不是最大则需要处理pcm数据 */
//       if (!is->muted && is->audio_buf && is->audio_volume == SDL_MIX_MAXVOLUME)
//           memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
//       else {
//           memset(stream, 0, len1);
//           // 3.调整音量
//           /* 如果处于mute状态则直接使用stream填0数据, 暂停时is->audio_buf = NULL */
//          if (!is->muted && is->audio_buf)
       /*3333333333333333333333333333333333333*/
       SDL_MixAudio(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1, is->volume);//volume最大128

//               SDL_MixAudioFormat(stream, (uint8_t *)is->audio_buf + is->audio_buf_index,
//                                  AUDIO_S16SYS, len1, is->volume);//volume最大128
//       }
       len -= len1;
       stream += len1;
       /* 更新is->audio_buf_index，指向audio_buf中未被拷贝到stream的数据（剩余数据）的起始位置 */
       is->audio_buf_index += len1;//由这个len1进行索引的增加，音频可读总长度
   }
   //已经写入音频的数据，不断更新audio_buf中的数据
   is->audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;

   /* Let's assume the audio driver that is used by SDL has two periods. */
//   if (!isnan(is->audio_clock)) {
//       set_clock_at(&is->audclk, is->audio_clock - (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size) / is->audio_tgt.bytes_per_sec, is->audio_clock_serial, audio_callback_time / 1000000.0);
//       sync_clock_to_slave(&is->extclk, &is->audclk);
}

//先参考我们之前讲的sdl_pcm范例，初始化音频播放输出
int FFPlayer::audio_open(int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, AudioParams *audio_hw_params){

  SDL_AudioSpec wanted_spec, spec;
   //这是目标音频播放器的参数
   //音频参数设置SDL_Audio      wanted_spec参数是SpecSDL 硬件抽象层参数
   wanted_spec.freq = wanted_sample_rate;//采样频率
   wanted_spec.format = AUDIO_S16SYS;    //采样点格式
   wanted_spec.channels = wanted_nb_channels;//2通道
   wanted_spec.silence = 0;
   wanted_spec.samples = 2048;//23.2ms -> 46.4ms 每次读取的采样数量，多久产生一次回调和samples
   wanted_spec.callback = sdl_audio_callback;//回调函数
   wanted_spec.userdata = this;
//  SDL_OpenAudioDevice
   //打开音频设备,在打开音频设备是进行sdl_audio_callback音频触发
  /*111111111111111111111111111*/
 if(SDL_OpenAudio(&wanted_spec,NULL) != 0)
 {
     printf("Fail to open audio device\n");
     return -1;
 }
 //wanted_spec是期望函数，spec是实际的参数，wanted_spec和spec都是SDL中的结构
 //此处的audio_hw_params是FFmpeg中的参数，输出参数供上级函数使用
 //audio_hw_params保存的参数，就是转成后的格式，核心硬件参数
 //把硬件确认的 wanted_spec 翻译成 audio_hw_params（给上层看），这样可以通过调用callback进行消耗数据data
 audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
 audio_hw_params->freq = wanted_spec.freq;
 audio_hw_params->channel_layout = wanted_channel_layout;
 audio_hw_params->channels = wanted_spec.channels;
 //audio_hw_params->frame_size这里只是计算一个采样点占用的字节数
 audio_hw_params->frame_size = av_samples_get_buffer_size(NULL,audio_hw_params->channels,1,audio_hw_params->fmt,1);
 //每一秒占用的字节数
 audio_hw_params->bytes_per_sec = av_samples_get_buffer_size(NULL,audio_hw_params->channels,audio_hw_params->freq,audio_hw_params->fmt,1);
 if(audio_hw_params->bytes_per_sec <= 0 || audio_hw_params->frame_size <= 0){
     return -1;
 }
 //比如2帧数据，一帧就是1024个采样点，1024*2*2 *2= 8192字节
 return wanted_spec.size;  //SDL内部缓存的数据字节，samples*channels*byte_per_sample


}
//时钟函数
int FFPlayer::init(Clock *c)//初始化时钟结构体,就是将时钟的pts置为NAN
{
    setClock(c,NAN);
}
void FFPlayer::setClock(Clock *c,double pts)//设置时钟时间戳并记录当时的时间
{
  double time = av_gettime_relative()/1000000.0;
  setClock_at(c,pts,time);//设置时钟在某一时刻
}
void FFPlayer::setClock_at(Clock *c,double pts,double time)//设置时钟时间，将参数赋予给时钟
{
   c->pts = pts;//设置时钟时间戳
   c->last_updated = time;//记录上次更新时间
   c->pts_drift = pts - time;//设置根据系统时间依附于的差值
}
double FFPlayer::get_clock(Clock *c)//返回时钟时间
{
  double time = av_gettime_relative()/1000000.0;
     return c->pts_drift + time;
}
int FFPlayer::ffp_screenshot_l(char *screen_path)
{
    // 存在视频的情况下才能截屏
    if(video_st && !req_screenshot_) {
        if(screen_path_) {
            free(screen_path_);
            screen_path_ = NULL;
        }
        screen_path_ = strdup(screen_path);//获取存放文件的地址
        req_screenshot_ = true;
    }
    return 0;
}

void FFPlayer::screenshot(AVFrame *frame)
{
    //根据截屏请求标志位进行截屏函数调用
    if(req_screenshot_) {
        ScreenShot shot;
        int ret = -1;
        if(frame) {
            //其实就是把当前帧frame进行存入到指定的文件地址
            ret = shot.SaveJpeg(frame, screen_path_, 70);
        }
        // 如果正常则ret = 0; 异常则为 < 0
        ffp_notify_msg4(this, FFP_MSG_SCREENSHOT_COMPLETE, ret, 0, screen_path_, strlen(screen_path_) + 1);
        // 截屏完毕后允许再次截屏
        req_screenshot_ = false;
    }
}



void FFPlayer::ffp_set_playback_rate(float rate)
{
    pf_playback_rate = rate;
    pf_playback_rate_changed = 1;
}

float FFPlayer::ffp_get_playback_rate()
{
    return pf_playback_rate;
}

bool FFPlayer::is_normal_playback_rate()
{
    if(pf_playback_rate > 0.99 && pf_playback_rate < 1.01) {
        return true;
    } else {
        return false;
    }
}

int FFPlayer::ffp_get_playback_rate_change()
{
    return pf_playback_rate_changed;
}

void FFPlayer::ffp_set_playback_rate_change(int change)
{
    pf_playback_rate_changed = change;
}

void FFPlayer::ffp_set_playback_volume(int value)
{
    value = av_clip(value, 0, 100);
    value = av_clip(SDL_MIX_MAXVOLUME *  value / 100, 0, SDL_MIX_MAXVOLUME);
    volume = value;
    qDebug() << "audio_volume: " << volume  ;
}

void FFPlayer::check_play_finish()
{
    //    LOG(INFO) << "eof: " << eof << ", audio_no_data: " << audio_no_data  ;
    if(eof == 1) { // 1. av_read_frame已经返回了AVERROR_EOF
        if(audio_stream >= 0 && video_stream >= 0) { // 2.1 音频、视频同时存在的场景
            if(audio_no_data == 1 && video_no_data == 1) {
                // 发送停止
                ffp_notify_msg1(this, FFP_MSG_PLAY_FNISH);
            }
            return;
        }
        if(audio_stream >= 0) { // 2.2 只有音频存在
            if(audio_no_data == 1) {
                // 发送停止
                ffp_notify_msg1(this, FFP_MSG_PLAY_FNISH);
            }
            return;
        }
        if(video_stream >= 0) { // 2.3 只有视频存在
            if(video_no_data == 1) {
                // 发送停止
                ffp_notify_msg1(this, FFP_MSG_PLAY_FNISH);
            }
            return;
        }
    }
}
int FFPlayer::get_target_frequency()
{
    return audio_tag.freq;
}

int FFPlayer::get_target_channels()
{
    return audio_tag.channels;
}
