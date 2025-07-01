/**
* @projectName   08-02-encode_video
* @brief         视频编码，从本地读取YUV数据进行H264编码
* @author        Liao Qingfu
* @date          2020-04-16
*/

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavutil/rational.h>
#include <libavcodec/avcodec.h>         //音频重采样
#include <libswresample/swresample.h>
}



int64_t get_time()
{
    return av_gettime_relative() / 1000;  // 换算成毫秒
}

//参数1：解码器上下文
//参数2：待编码的帧
//参数3：编码完成的帧
//参数4：目标文件的指针
static int encode(AVCodecContext* enc_ctx, AVFrame* frame, AVPacket* pkt,
    FILE* outfile)
{
    int ret;

    /* 发送帧到编码器 */
    //if (frame)
        //printf("Send frame %3"PRId64"\n", frame->pts);
    /* 通过查阅代码，使用x264进行编码时，具体缓存帧是在x264源码进行，
     * 不会增加avframe对应buffer的reference*/

     // 将帧数据发送给编码器进行编码
    ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0)
    {
        fprintf(stderr, "Error sending a frame for encoding\n");
        return -1;
    }

    while (ret >= 0)
    {
        // 从编码器接收已编码的数据包
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return 0;
        }
        else if (ret < 0)
        {
            fprintf(stderr, "Error encoding audio frame\n");
            return -1;
        }

        //if (pkt->flags & AV_PKT_FLAG_KEY)
            //printf("Write packet flags:%d pts:%3"PRId64" dts:%3"PRId64" (size:%5d)\n",
                //pkt->flags, pkt->pts, pkt->dts, pkt->size);
       // if (!pkt->flags)
           // printf("Write packet flags:%d pts:%3"PRId64" dts:%3"PRId64" (size:%5d)\n",
           //     pkt->flags, pkt->pts, pkt->dts, pkt->size);

       /*
            ptr：指向要写入的数据的指针。在你的例子中是 pkt->data，即编码后的数据（如视频的压缩数据）。
            size：每个数据元素的大小，通常是 1 字节（1），表示每次写入 1 字节的数据。
            count：要写入的数据元素的个数，通常是 pkt->size，即数据的总字节数。
            stream：文件指针，指定数据写入的目标文件。在你的例子中是 outfile，即目标文件的指针
       */
       fwrite(pkt->data, 1, pkt->size, outfile);
    }
    return 0;
}
/**
 * @brief 提取测试文件：ffmpeg -i test_1280x720.flv -t 5 -r 25 -pix_fmt yuv420p yuv420p_1280x720.yuv
 *           参数输入: yuv420p_1280x720.yuv yuv420p_1280x720.h264 libx264
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char** argv)
{
    char* in_yuv_file = NULL;   // 输入YUV文件
    char* out_h264_file = NULL; // 打开输出文件的句柄
    FILE* infile = NULL;        // 输入文件句柄
    FILE* outfile = NULL;       //目标文件的指针

    const char* codec_name = NULL;      //  编码器名称
    const AVCodec* codec = NULL;        //  解码器
    AVCodecContext* codec_ctx = NULL;   //  解码器上下文
    AVFrame* frame = NULL;              //  待编码的帧
    AVPacket* pkt = NULL;               //  编码后的帧
    int ret = 0;

    if (argc < 4)
    {
        fprintf(stderr, "Usage: %s <input_file out_file codec_name >, argc:%d\n",
            argv[0], argc);
        return 0;
    }
    in_yuv_file = argv[1];      // 输入YUV文件
    out_h264_file = argv[2];
    codec_name = argv[3];

    /* 查找指定的编码器 */
    codec = avcodec_find_encoder_by_name(codec_name);
    if (!codec)
    {
        fprintf(stderr, "Codec '%s' not found\n", codec_name);
        exit(1);
    }

    //初始化解码器上下文
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx)
    {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }


    /* 设置分辨率*/
    codec_ctx->width = 1280;
    codec_ctx->height = 720;
    /* 设置time base */
    //codec_ctx->time_base = (AVRational){ 1, 25 };//时间基准,表示每 25 帧对应 1 秒，因此每帧的时间间隔为 1/25 秒
    //codec_ctx->framerate = (AVRational){ 25, 1 };//帧率,表示视频的帧率为 25 帧每秒。
    codec_ctx->time_base.num = 1;  // 设置分子
    codec_ctx->time_base.den = 25; // 设置分母

    codec_ctx->framerate.num = 25; // 设置分子
    codec_ctx->framerate.den = 1;  // 设置分母
    /* 设置I帧间隔
     * 如果frame->pict_type设置为AV_PICTURE_TYPE_I, 则忽略gop_size的设置，一直当做I帧进行编码
     */
    codec_ctx->gop_size = 25;   // I帧间隔
    codec_ctx->max_b_frames = 2; // 表示最多可以插入 2 个 B 帧,如果不想包含B帧则设置为0
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    //设置 H.264 编码器（AV_CODEC_ID_H264）的相关参数
    if (codec->id == AV_CODEC_ID_H264)
    {
        // 相关的参数可以参考libx264.c的 AVOption options
        // ultrafast all encode time:2270ms,最快的编码速度，编码时间最短，但生成的视频文件较大，压缩效率差。
        // medium all encode time:5815ms,默认的编码速度与压缩效率之间的平衡，生成的视频质量较好，压缩效率适中。
        // veryslow all encode time:19836ms,编码时间最长，生成的视频质量最好，压缩效率也最好。
        ret = av_opt_set(codec_ctx->priv_data, "preset", "medium", 0);
        if (ret != 0)
        {
            printf("av_opt_set preset failed\n");
        }

        //baseline：适用于低延迟、低复杂度的应用，通常用于移动设备或视频会议。
        //main：适用于大多数标准应用，如在线视频播放，兼容性较好。
        //high：适用于需要高质量视频的应用，通常用于高清或蓝光光盘等
        ret = av_opt_set(codec_ctx->priv_data, "profile", "main", 0); // 默认是high
        if (ret != 0)
        {
            printf("av_opt_set profile failed\n");
        }


        //zerolatency：适用于低延迟的应用，比如实时直播。选择此设置时，编码器将尽量减少延迟，确保视频尽可能快地输出，而不进行复杂的压缩优化。
        //film：适用于电影或高质量视频。启用该设置时，编码器会优先优化图像质量，减少压缩损失，适合质量要求高的场景。
        //fastdecode：优化解码性能，适用于解码设备性能有限的场景。
        //psnr：优化编码参数以提高峰值信噪比（PSNR），用于追求视觉质量的编码。        
        //ret = av_opt_set(codec_ctx->priv_data, "tune","zerolatency",0); // 直播是才使用该设置
//        ret = av_opt_set(codec_ctx->priv_data, "tune","film",0); //  画质film
        if (ret != 0)
        {
            printf("av_opt_set tune failed\n");
        }
    }

    /*
     * 设置编码器参数
    */
    /* 设置bitrate */
    //这行代码是设置视频编码器的比特率（bit rate）。
    codec_ctx->bit_rate = 3000000;
    //    codec_ctx->rc_max_rate = 3000000;
    //    codec_ctx->rc_min_rate = 3000000;
    //    codec_ctx->rc_buffer_size = 2000000;
    //    codec_ctx->thread_count = 4;  // 开了多线程后也会导致帧输出延迟, 需要缓存thread_count帧后再编程。
    //    codec_ctx->thread_type = FF_THREAD_FRAME; // 并 设置为FF_THREAD_FRAME
    /* 对于H264 AV_CODEC_FLAG_GLOBAL_HEADER  设置则只包含I帧，此时sps pps需要从codec_ctx->extradata读取
        *  不设置则每个I帧都带 sps pps sei
        */
        //    codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; // 存本地文件时不要去设置

            /* 将codec_ctx和codec进行绑定 */
            //初始化并打开指定的编码器或解码器
    ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0)
    {
        //fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
        exit(1);
    }

    printf("thread_count: %d, thread_type:%d\n", codec_ctx->thread_count, codec_ctx->thread_type);
    // 打开输入和输出文件
    // 打开YUV文件
    infile = fopen(in_yuv_file, "rb");
    if (!infile)
    {
        fprintf(stderr, "Could not open %s\n", in_yuv_file);
        exit(1);
    }

    // 输出文件
    outfile = fopen(out_h264_file, "wb");
    if (!outfile)
    {
        fprintf(stderr, "Could not open %s\n", out_h264_file);
        exit(1);
    }

    // 分配pkt和frame
    pkt = av_packet_alloc();
    if (!pkt)
    {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }

    frame = av_frame_alloc();
    if (!frame)
    {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }

    // 为frame分配buffer
    frame->format = codec_ctx->pix_fmt;
    frame->width = codec_ctx->width;
    frame->height = codec_ctx->height;
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0)
    {
        fprintf(stderr, "Could not allocate the video frame data\n");
        exit(1);
    }
    // 计算出每一帧的数据 像素格式 * 宽 * 高
    // 1382400
    int frame_bytes = av_image_get_buffer_size((AVPixelFormat)frame->format, frame->width,
        frame->height, 1);
    printf("frame_bytes %d\n", frame_bytes);
    uint8_t* yuv_buf = (uint8_t*)malloc(frame_bytes);
    if (!yuv_buf)
    {
        printf("yuv_buf malloc failed\n");
        return 1;
    }
    int64_t begin_time = get_time();
    int64_t end_time = begin_time;
    int64_t all_begin_time = get_time();
    int64_t all_end_time = all_begin_time;
    int64_t pts = 0;
    printf("start enode\n");
    for (;;)
    {
        memset(yuv_buf, 0, frame_bytes);
        /*
            参数1：用来存储从文件中读取的数据
            参数2：每次读取 1 字节
            参数3：要读取的字节数（即读取的总字节数）
            参数4：输入文件的文件流
        */
        size_t read_bytes = fread(yuv_buf, 1, frame_bytes, infile);
        if (read_bytes <= 0)
        {
            printf("read file finish\n");
            break;
        }
        /* 确保该frame可写, 如果编码器内部保持了内存参考计数，则需要重新拷贝一个备份
            目的是新写入的数据和编码器保存的数据不能产生冲突
        */
        int frame_is_writable = 1;              // 假设frame可写
        if (av_frame_is_writable(frame) == 0)    // 检查frame是否可写
        {
            // 这里只是用来测试
            printf("the frame can't write, buf:%p\n", frame->buf[0]);
            if (frame->buf && frame->buf[0])        // 打印referenc-counted，必须保证传入的是有效指针
                printf("ref_count1(frame) = %d\n", av_buffer_get_ref_count(frame->buf[0]));
            frame_is_writable = 0;
        }

        ret = av_frame_make_writable(frame);    // 强制使frame可写
        if (frame_is_writable == 0)              // 如果frame原本不可写
        {
            // 这里只是用来测试
            printf("av_frame_make_writable, buf:%p\n", frame->buf[0]);
            if (frame->buf && frame->buf[0])        // 打印referenc-counted，必须保证传入的是有效指针
                printf("ref_count2(frame) = %d\n", av_buffer_get_ref_count(frame->buf[0]));
        }
        // 检查是否成功使frame可写
        if (ret != 0)
        {
            printf("av_frame_make_writable failed, ret = %d\n", ret);
            break;
        }

        // 将YUV数据填充到frame
        int need_size = av_image_fill_arrays(frame->data, frame->linesize, yuv_buf,
            (AVPixelFormat)frame->format,
            frame->width, frame->height, 1);
        if (need_size != frame_bytes)
        {
            printf("av_image_fill_arrays failed, need_size:%d, frame_bytes:%d\n",
                need_size, frame_bytes);
            break;
        }
        // 更新时间戳，每帧时间差为40
        pts += 40;

        // 设置pts
        frame->pts = pts;                               // 使用采样率作为pts的单位，具体换算成秒 pts*1/采样率
        begin_time = get_time();                        // 记录开始时间
        ret = encode(codec_ctx, frame, pkt, outfile);   // 调用encode函数进行编码
        end_time = get_time();                          // 记录结束时间
        printf("encode time:%lldms\n", end_time - begin_time);
        if (ret < 0)
        {
            printf("encode failed\n");
            break;
        }
    }

    /* 冲刷编码器 */
    encode(codec_ctx, NULL, pkt, outfile);
    all_end_time = get_time();
    printf("all encode time:%lldms\n", all_end_time - all_begin_time);
    // 关闭文件
    fclose(infile);
    fclose(outfile);

    // 释放内存
    if (yuv_buf)
    {
        free(yuv_buf);
    }

    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);

    printf("main finish, please enter Enter and exit\n");
    getchar();
    return 0;
}
