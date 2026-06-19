#include "screenshot.h"
//#include "easylogging++.h"
#include <QDebug>
//分配一帧需要格式转换的视频帧
AVFrame *allocate_sws_frame(AVCodecContext *enc_ctx)
{
    int ret = 0;
    AVFrame *sws_frame = av_frame_alloc();
    if(sws_frame)
    {
        sws_frame->format = enc_ctx->pix_fmt;
        sws_frame->width = enc_ctx->width;
        sws_frame->height = enc_ctx->height;
        sws_frame->pict_type = AV_PICTURE_TYPE_NONE;
        ret = av_frame_get_buffer(sws_frame, 32);   // 分配buffer
        if(ret <0)
        {
            av_frame_free(&sws_frame);
            return NULL;
        }
    }
    return sws_frame;
}

ScreenShot::ScreenShot()
{

}
//开始保存
int ScreenShot::SaveJpeg(AVFrame *src_frame, const char *file_name, int jpeg_quality)
{   //主要流程就是将一帧frame用jpeg编码器编码成jpg格式并写入到本地进行存储
    AVFormatContext* ofmt_ctx = NULL;
    AVOutputFormat* fmt = NULL;
    AVStream* video_st = NULL;
    AVCodecContext* enc_ctx = NULL;
    AVCodec* codec = NULL;
    AVFrame* picture = NULL;
    AVPacket *pkt = NULL;
    int got_picture = 0;
    int ret = 0;
    struct  SwsContext *img_convert_ctx = NULL;
    //输出上下文的创建
    ofmt_ctx = avformat_alloc_context();
    //Guess format,
    //图片格式
    fmt = av_guess_format("mjpeg", NULL, NULL);
    ofmt_ctx->oformat = fmt;
    //Output URL
    //打开保存管道，放包的位置，以后根据ofmt_ctx就可知道放包的位置
    if (avio_open(&ofmt_ctx->pb, file_name, AVIO_FLAG_READ_WRITE) < 0){
       qDebug()  <<"Couldn't open output file.";
        ret = -1;
        goto fail;
    }
    //创建新流
    video_st = avformat_new_stream(ofmt_ctx, 0);
    if (video_st==NULL){
        ret = -1;
        goto fail;
    }
    enc_ctx = video_st->codec;
    enc_ctx->codec_id = AV_CODEC_ID_MJPEG;      // mjpeg支持的编码器
    enc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    enc_ctx->pix_fmt = AV_PIX_FMT_YUVJ420P; // AV_CODEC_ID_MJPEG 支持的像素格式

    enc_ctx->width  = src_frame->width;
    enc_ctx->height = src_frame->height;

    enc_ctx->time_base.num = 1;
    enc_ctx->time_base.den = 25;
    //Output some information
    av_dump_format(ofmt_ctx, 0, file_name, 1);

    codec = avcodec_find_encoder(enc_ctx->codec_id);
    if (!codec){
        qDebug()  << "jpeg Codec not found.";
        ret = -1;
        goto fail;
    }
    if (avcodec_open2(enc_ctx, codec,NULL) < 0){
        qDebug()  << "Could not open jpeg codec.";
        ret = -1;
        goto fail;
    }
    ret = avcodec_parameters_from_context(video_st->codecpar, enc_ctx);
    if(ret < 0) {
      qDebug()  <<"avcodec_parameters_from_context failed";
        ret = -1;
        goto fail;
    }
    if(src_frame->format != enc_ctx->pix_fmt) {
        img_convert_ctx = sws_getContext(enc_ctx->width, enc_ctx->height,
                                         (enum AVPixelFormat)src_frame->format, enc_ctx->width, enc_ctx->height,
                                         enc_ctx->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);
        if (!img_convert_ctx) {
           qDebug()  << "Impossible to create scale context for the conversion fmt:"
                       << av_get_pix_fmt_name((enum AVPixelFormat)src_frame->format)
                       << ", s:" <<  enc_ctx->width << "x" << enc_ctx->height << " -> fmt:" << av_get_pix_fmt_name(enc_ctx->pix_fmt)
                       << ", s:" <<  enc_ctx->width << "x" << enc_ctx->height ;
            ret = -1;
            goto fail;
        }
    }

    if(jpeg_quality > 0)
    {
        if(jpeg_quality > 100)
            jpeg_quality = 100;

        enc_ctx->qcompress = (float)jpeg_quality/100.f; // 0~1.0, default is 0.5
        enc_ctx->qmin = 2;
        enc_ctx->qmax = 31;
        enc_ctx->max_qdiff = 3;

       qDebug()  <<"JPEG quality is: %d" << jpeg_quality;
    }
    pkt = av_packet_alloc();
    //Write Header
    ret = avformat_write_header(ofmt_ctx, NULL);
    if(ret < 0) {
      qDebug()  <<"avformat_write_header failed";
        ret = -1;
        goto fail;
    }

    if(img_convert_ctx)     // 如果需要转换pix_fmt
    {
        // 分配转换后的frame
        picture = allocate_sws_frame(enc_ctx);
        /* make sure the frame data is writable */
        ret = av_frame_make_writable(picture);
        ret = sws_scale(img_convert_ctx, (const uint8_t **) src_frame->data, src_frame->linesize, 0, src_frame->height,
                        picture->data, picture->linesize);
        picture->pts = 0;
        //这个是视频编码的核心API，用来将frame编码成pkt，其中got_pictures是返回值
        ret = avcodec_encode_video2(enc_ctx, pkt, picture, &got_picture);
    }
    else
    {
        ret = avcodec_encode_video2(enc_ctx, pkt, src_frame, &got_picture);
    }

    if(ret < 0){
    qDebug()  <<"avcodec_encode_video2 Error.";
        ret = -1;
        goto fail;
    }
    if (got_picture==1){
        pkt->stream_index = video_st->index;
        ret = av_write_frame(ofmt_ctx, pkt);//这个ofmt_ctx中已经保存了放包的位置
        if(ret < 0) {
          qDebug()  <<"av_write_frame Error.";
            ret = -1;
            goto fail;
        }
    }else {
        qDebug()  <<"no got_picture";
        ret = -1;
        goto fail;
    }
    ret = 0;
fail:
    //Write Trailer
    ret = av_write_trailer(ofmt_ctx);
    if(ret < 0)
       qDebug()  <<"av_write_trailer Error.";
    if(pkt)
        av_packet_free(&pkt);
    if (enc_ctx)
        avcodec_close(enc_ctx);
    if(picture)
        av_frame_free(&picture);
    if(ofmt_ctx && ofmt_ctx->pb)
        avio_close(ofmt_ctx->pb);
    if(ofmt_ctx)
        avformat_free_context(ofmt_ctx);
    if(img_convert_ctx)
        sws_freeContext(img_convert_ctx);

    return ret;
}

