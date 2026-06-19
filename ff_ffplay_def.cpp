#include "ff_ffplay_def.h"
#define MAX_VIDEO_PACKETS 166
#define MAX_AUDIO_PACKETS 166
static AVPacket flush_pkt = {0};
static int packet_queue_put_private(PacketQueue *q,AVPacket *pkt)
{
    MyAVPacketList *pkt1;
    if(q->abort_request){ //如果终止，则放入失败
        return -1;
    }
    pkt1 = (MyAVPacketList*)av_malloc(sizeof(MyAVPacketList));
    if(!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;
    if(pkt == &flush_pkt)//如果放入的是flush_pkt,需要增加队列的播放序列号，以区分不连续的两段数据
    {
        q->serial++;//在这里队列时序已经自加了
        printf("q->serial = %d\n",q->serial);
    }
    pkt1->serial = q->serial; //用队列序列号标记节点
    //队列操作：如果last_pkt为空，说明队列是空的，新增节点为队头;
    //否则,队列有数据，则让原队尾的next为新增节点，最后将队尾指向新增节点;
    if(!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;

    //队列属性操作：新增节点数，cache大小，cache总时长，用来控制队列的大小
    q->nb_packets++;
    q->size += pkt1->pkt.size + sizeof(*pkt1);
    q->duration += pkt1->pkt.duration;
    //发出信号，表明当前队列已经有数据了，通知等待中的读线程可以读数据了
    SDL_CondSignal(q->cond);
//    if (pkt != &flush_pkt && ret < 0)
//        av_packet_unref(pkt);       //放入失败，释放AVPacket
    return 0;
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    int ret;

    SDL_LockMutex(q->mutex);
    ret = packet_queue_put_private(q, pkt);//主要实现
    SDL_UnlockMutex(q->mutex);

    if (pkt != &flush_pkt && ret < 0)
        av_packet_unref(pkt);       //放入失败，释放AVPacket

    return ret;
}

static int packet_queue_put_nullpacket(PacketQueue *q, int stream_index)
{
    AVPacket pkt1, *pkt = &pkt1;
    av_init_packet(pkt);
    pkt->data = NULL;
    pkt->size = 0;
    pkt->stream_index = stream_index;
    return packet_queue_put(q, pkt);
}


/* packet queue handling */
int packet_queue_init(PacketQueue *q)
{
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    if (!q->mutex) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    q->cond = SDL_CreateCond();
    if (!q->cond) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    q->abort_request = 1;
    return 0;
}


void packet_queue_flush(PacketQueue *q)
{
    MyAVPacketList *pkt, *pkt1;

    SDL_LockMutex(q->mutex);
    for (pkt = q->first_pkt; pkt; pkt = pkt1) {
        pkt1 = pkt->next;
        av_packet_unref(&pkt->pkt);
        av_freep(&pkt);
    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
    q->nb_packets = 0;
    q->size = 0;
    q->duration = 0;
    SDL_UnlockMutex(q->mutex);

}
void frame_queue_flush(FrameQueue *f){
    int fsize = 0;
    SDL_LockMutex(f->mutex);
    fsize = f->size;
   for(int i = 0;i < FRAME_QUEUE_SIZE ;i++){
     Frame *vp = &f->queue[f->rindex];
     if(vp->frame)
      av_frame_unref(vp->frame);	/* 释放数据 */
  //  frame_queue_unref_item(&f->queue[f->rindex]);

   }
      f->size=0;
//     memset(f, 0, sizeof(FrameQueue));

    SDL_CondSignal(f->cond);//已经冲刷完了，唤醒可以重头开始了
    SDL_UnlockMutex(f->mutex);

}

void packet_queue_destroy(PacketQueue *q)
{
    packet_queue_flush(q); //先清除所有的节点
    SDL_CondSignal(q->cond);
    SDL_DestroyMutex(q->mutex);
    SDL_DestroyCond(q->cond);
}


void packet_queue_abort(PacketQueue *q)
{
    SDL_LockMutex(q->mutex);

    q->abort_request = 1;       // 请求退出

    SDL_CondSignal(q->cond);    //释放一个条件信号

    SDL_UnlockMutex(q->mutex);
}

 void packet_queue_start(PacketQueue *q)
{
    SDL_LockMutex(q->mutex);
    q->abort_request = 0;
    packet_queue_put_private(q, &flush_pkt); //这里放入了一个flush_pkt
    SDL_UnlockMutex(q->mutex);
}

 int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial)
{
    MyAVPacketList *pkt1;
    int ret;

    SDL_LockMutex(q->mutex);    // 加锁

    for (;;) {
        if (q->abort_request) {
            ret = -1;
            break;
        }

        pkt1 = q->first_pkt;    //MyAVPacketList *pkt1; 从队头拿数据
        if (pkt1) {     //队列中有数据
            q->first_pkt = pkt1->next;  //队头移到第二个节点
            if (!q->first_pkt)//如果队列第二个包为空
                q->last_pkt = NULL;//那么到尽头了
            q->nb_packets--;    //节点数减1
            q->size -= pkt1->pkt.size + sizeof(*pkt1);  //cache大小扣除一个节点
            q->duration -= pkt1->pkt.duration;  //总时长扣除一个节点
            //返回AVPacket，这里发生一次AVPacket结构体拷贝，AVPacket的data只拷贝了指针
            *pkt = pkt1->pkt;
            if (serial) //如果需要输出serial，把serial输出
                *serial = pkt1->serial;//在这里的包已经有时序了
            av_free(pkt1);      //释放节点内存,只是释放节点，而不是释放AVPacket
            ret = 1;
            break;
        } else if (!block) {    //队列中没有数据，且非阻塞调用
            ret = 0;
            break;
        } else {    //队列中没有数据，且阻塞调用
            //这里没有break。for循环的另一个作用是在条件变量满足后重复上述代码取出节点
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);  // 释放锁
    return ret;
}

static void frame_queue_unref_item(Frame *vp)
{
    av_frame_unref(vp->frame);	/* 释放数据 */
//    avsubtitle_free(&vp->sub);
}

/* 初始化FrameQueue，视频和音频keep_last设置为1，字幕设置为0 */
int frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size)
{
    int i;
    memset(f, 0, sizeof(FrameQueue));
    if (!(f->mutex = SDL_CreateMutex())) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    if (!(f->cond = SDL_CreateCond())) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    f->pktq = pktq;
    f->max_size = FFMIN(max_size, FRAME_QUEUE_SIZE);
//    f->keep_last = !!keep_last;
    for (i = 0; i < f->max_size; i++)
        if (!(f->queue[i].frame = av_frame_alloc())) // 分配AVFrame结构体内存
            return AVERROR(ENOMEM);
    return 0;
}

void frame_queue_destroy(FrameQueue *f)
{
    int i;
    for (i = 0; i < f->max_size; i++) {
        Frame *vp = &f->queue[i];
        // 释放对vp->frame中的数据缓冲区的引用，注意不是释放frame对象本身
        frame_queue_unref_item(vp);
         // 释放vp->frame对象
        av_frame_free(&vp->frame);
    }
    SDL_CondSignal(f->cond);
    SDL_DestroyMutex(f->mutex);
    SDL_DestroyCond(f->cond);
}

 void frame_queue_signal(FrameQueue *f)
{
    SDL_LockMutex(f->mutex);
    SDL_CondSignal(f->cond);
    SDL_UnlockMutex(f->mutex);
}
Frame *fream_queue_peek_next(FrameQueue *f)
{
    return &f->queue[(f->rindex + 1) % f->max_size];
}
 //获取可写帧
Frame *fream_queue_peek_last(FrameQueue *f)//这个函数就是看一下队列之中是否还有数据
{
    return &f->queue[f->rindex];
}
Frame *frame_queue_peek_writable(FrameQueue *f)
{
  /*wait until we have space to put a new frame*/
    SDL_LockMutex(f->mutex);
    while(f->size >= f->max_size && !f->pktq->abort_request)
    {
        SDL_CondWait(f->cond,f->mutex);//当解码队列满时进行无限等待,这时候当flush的时候，唤醒队列然后继续判断退出（合理）
                                        //那么其实flush针对的还是这个函数
    }
    SDL_UnlockMutex(f->mutex);
    if(f->pktq->abort_request)//检查是不是要退出
    {
        return NULL;
    }
    return &f->queue[f->windex];

}
Frame *frame_queue_peek_readable(FrameQueue *f)
{
    /*wait until we have a readable a new frame*/
      SDL_LockMutex(f->mutex);//这个mutex锁只针对framequeue队列成员
      while(f->size <= 0 && !f->pktq->abort_request){
           SDL_CondWait(f->cond,f->mutex);//没包可读时等待，唤醒之后也等待，除非请求退出时，说明该退出了
      }//退出直接结束了，不用再等待了
      SDL_UnlockMutex(f->mutex);
      if(f->pktq->abort_request)//检查是不是要退出
      {
          return NULL;
      }
      return &f->queue[(f->rindex) % f->max_size];
}
//获
void frame_queue_push(FrameQueue *f)
{
    if (++f->windex == f->max_size)
        f->windex = 0;
    SDL_LockMutex(f->mutex);
    f->size++;             //拥有wait等待时可以节省cpu资源，和没有wait等待的区别就是唤醒之后会判断看一看while循环是否还满足线程等待
    SDL_CondSignal(f->cond);// 当_readable在等待时则可以唤醒,正在等待的会再次看看条件，当条件不满足时再退出
    SDL_UnlockMutex(f->mutex);
}
void frame_queue_next(FrameQueue *f)
{
//    if (f->keep_last && !f->rindex_shown) {
//        f->rindex_shown = 1; // 第一次进来没有更新，对应的frame就没有释放
//        return;
//    }

    frame_queue_unref_item(&f->queue[f->rindex]);
    if (++f->rindex == f->max_size)
        f->rindex = 0;
    SDL_LockMutex(f->mutex);
    f->size--;
    SDL_CondSignal(f->cond);
    SDL_UnlockMutex(f->mutex);



}


int frame_queue_nb_remaining(FrameQueue *f)
{

      return f->size - f->rindex;	// 注意这里为什么要减去f->rindex_shown

}
//获取当前frame，在调用该函数前先调用frame_queue_nb_remaining确保有frame可读
Frame *frame_queue_peek(FrameQueue *f)
{
    return &f->queue[f->rindex % f->max_size] ;
}
int stream_has_enough_packets(AVStream *st,int stream_id,PacketQueue *queue){
    if(!st||stream_id<0){
        return -1;
    }
    if(st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
        return queue->nb_packets ==  MAX_VIDEO_PACKETS;
    }
    else if(st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
        return queue->nb_packets ==  MAX_AUDIO_PACKETS;
    }
    return 0;//只有满的时候去等其他时候不必去等，因为这个适用于seek和pause,其他情况正常运行不用关心这个
}

//int init(Clock *c)//初始化时钟结构体,就是将时钟的pts置为NAN
//{
//    setClock(c,NAN);
//}
//void setClock(Clock *c,double pts)//设置时钟时间戳并记录当时的时间
//{
//  double time = av_gettime_relative()/1000000.0;
//  setClock_at(c,pts,time);//设置时钟在某一时刻
//}
//void setClock_at(Clock *c,double pts,double time)//设置时钟时间，将参数赋予给时钟
//{
//   c->pts = pts;//设置时钟时间戳
//   c->last_updated = time;//记录上次更新时间
//   c->pts_drift = pts - time;//设置根据系统时间依附于的差值
//}
//double get_clock(Clock *c)//返回时钟时间
//{
//  double time = av_gettime_relative()/1000000.0;
//     return c->pts_drift + time;
//}
