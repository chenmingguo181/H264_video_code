# H264_video_code
视频编码，从本地读取YUV数据进行H264编码
提取测试文件：ffmpeg -i test_1280x720.flv -t 5 -r 25 -pix_fmt yuv420p yuv420p_1280x720.yuv
参数输入: yuv420p_1280x720.yuv yuv420p_1280x720.h264 libx264
