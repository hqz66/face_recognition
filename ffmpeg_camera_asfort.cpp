#include <iostream>
#include <pthread.h>
#include <atomic>
#include <unistd.h>
#include <opencv2/opencv.hpp>
#include "init_asfort.h"
// #include "mat_queue_linux.h"
#include "mat_queue.h"

extern "C"
{
#include <libavutil/avutil.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/fifo.h>
}

using namespace std;
using namespace cv;

#define VIDEO_WIDTH 640
#define VIDEO_HEIGHT 480

int image_yuv420p_size;
AVFifoBuffer *m_videoFifo = NULL;
MHandle g_asfort_handle = NULL;
CMatQueue *opencv_queue = NULL;

pthread_mutex_t fifo_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t fifo_cond = PTHREAD_COND_INITIALIZER;

atomic<bool> stop_flag(false);

void *get_camera_data(void *arg)
{
    av_log_set_level(AV_LOG_DEBUG);
    avdevice_register_all();
    int ret = 0;
    char errors[1024];
    const char *devicename = "/dev/video0";
    // FILE *outfile = fopen("output.yuv", "wb");
    char video_size[10];
    const AVInputFormat *input_format_const = NULL;
    AVInputFormat *iformat = NULL;
    AVFormatContext *fmt_ctx = NULL;
    AVDictionary *options = NULL;
    SwsContext *img_convert_ctx = NULL;
    AVPacket pkt;

    // av_init_packet(&pkt);
    input_format_const = av_find_input_format("v4l2");
    iformat = (AVInputFormat *)input_format_const;
    if (iformat == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "%d av_find_input_format() failed\n", __LINE__);
        return NULL;
    }

    img_convert_ctx = sws_getContext(VIDEO_WIDTH, VIDEO_HEIGHT, AV_PIX_FMT_YUYV422,
                                     VIDEO_WIDTH, VIDEO_HEIGHT, AV_PIX_FMT_YUV420P,
                                     SWS_BICUBIC, NULL, NULL, NULL);
    if (!img_convert_ctx)
    {
        av_log(NULL, AV_LOG_ERROR, "%d sws_getContext() failed\n", __LINE__);
        return NULL;
    }
    m_videoFifo = av_fifo_alloc((unsigned int)30 * image_yuv420p_size);
    // uint8_t *out_buffer = (uint8_t *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, VIDEO_WIDTH, VIDEO_HEIGHT, 0));
    uint8_t *src_buffer[AV_NUM_DATA_POINTERS];
    uint8_t *dst_buffer[AV_NUM_DATA_POINTERS];
    int src_linesizes[AV_NUM_DATA_POINTERS];
    int dst_linesizes[AV_NUM_DATA_POINTERS];
    int ret1 = av_image_alloc(src_buffer, src_linesizes, VIDEO_WIDTH, VIDEO_HEIGHT, AV_PIX_FMT_YUYV422, 1);
    int ret2 = av_image_alloc(dst_buffer, dst_linesizes, VIDEO_WIDTH, VIDEO_HEIGHT, AV_PIX_FMT_YUV420P, 1);

    if (ret1 <= 0 || ret2 <= 0)
    {
        fprintf(stderr, "Could not allocate image\n");
        return NULL;
    }

    sprintf(video_size, "%dx%d", VIDEO_WIDTH, VIDEO_HEIGHT);
    av_dict_set(&options, "video_size", video_size, 0);

    if ((ret = avformat_open_input(&fmt_ctx, devicename, iformat, &options)) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "%d av_find_input_format() failed\n", __LINE__);
        return NULL;
    }

    while (!stop_flag.load() && (ret = av_read_frame(fmt_ctx, &pkt)) == 0)
    {
        memcpy(src_buffer[0], pkt.data, pkt.size);

        sws_scale(img_convert_ctx, src_buffer, src_linesizes, 0, VIDEO_HEIGHT, dst_buffer, dst_linesizes);
        pthread_mutex_lock(&fifo_mutex);
        if (av_fifo_space(m_videoFifo) > image_yuv420p_size)
        {
            av_fifo_generic_write(m_videoFifo, dst_buffer[0], VIDEO_WIDTH * VIDEO_HEIGHT, NULL);
            av_fifo_generic_write(m_videoFifo, dst_buffer[1], VIDEO_WIDTH * VIDEO_HEIGHT / 4, NULL);
            av_fifo_generic_write(m_videoFifo, dst_buffer[2], VIDEO_WIDTH * VIDEO_HEIGHT / 4, NULL);
        }
        pthread_mutex_unlock(&fifo_mutex);
        pthread_cond_broadcast(&fifo_cond);
        av_packet_unref(&pkt);
    }
    // fclose(outfile);
    avformat_close_input(&fmt_ctx);
    av_log(NULL, AV_LOG_DEBUG, "finish!\n");
    sws_freeContext(img_convert_ctx);
    return NULL;
}

void show_image(Mat &yuv420p)
{
    imshow("I420 Image", yuv420p);
    imwrite("i420.png", yuv420p);
    Mat imgBGR;
    cvtColor(yuv420p, imgBGR, COLOR_YUV2BGR_I420);
    imshow("BGR Image", imgBGR);
    imwrite("bgr.png", imgBGR);
    int left = 100, top = 100, right = 300, bottom = 300;
    rectangle(imgBGR, Point(left, top), Point(right, bottom), Scalar(0, 0, 255), 1);
    imshow("Face Rect", imgBGR);
    waitKey(0);
}

void init_engine()
{
    init_sdk();
    MRESULT res = MOK;
    MInt32 mask = ASF_FACE_DETECT | ASF_FACERECOGNITION | ASF_FACE3DANGLE;
    res = ASFInitEngine(ASF_DETECT_MODE_VIDEO, ASF_OP_0_ONLY, NSCALE, FACENUM, mask, &g_asfort_handle);
    if (res != MOK)
        printf("ASFInitEngine fail: %ld\n", res);
    else
        printf("ASFInitEngine sucess: %ld\n", res);
}

void *face_recognition(void *arg)
{
    CMatQueue *mat_queue = CMatQueue::getInstance();
    MRESULT res = MOK;
    ASVLOFFSCREEN offscreen = {0};
    ASF_MultiFaceInfo detectedFaces = {0};
    ASF_SingleFaceInfo SingleDetectedFaces = {0};

    init_engine();
    Mat yuv420p(VIDEO_HEIGHT + VIDEO_HEIGHT / 2, VIDEO_WIDTH, CV_8UC1);
    // av_log(NULL, AV_LOG_DEBUG, "%s %d av_fifo_size(m_videoFifo) = %d \n", __func__, __LINE__, av_fifo_size(m_videoFifo));

    while (!stop_flag.load())
    {
        pthread_mutex_lock(&fifo_mutex);
        while(av_fifo_size(m_videoFifo) < image_yuv420p_size)
        {
            pthread_cond_wait(&fifo_cond, &fifo_mutex);
        }
        av_fifo_generic_read(m_videoFifo, yuv420p.data, VIDEO_WIDTH * VIDEO_HEIGHT * 3 / 2, NULL);
        pthread_mutex_unlock(&fifo_mutex);

        // show_image(yuv420p);
        ColorSpaceConversion(VIDEO_WIDTH, VIDEO_HEIGHT, ASVL_PAF_I420, yuv420p.data, offscreen);
        res = ASFDetectFacesEx(g_asfort_handle, &offscreen, &detectedFaces);
        if (res != MOK && detectedFaces.faceNum > 0)
        {
            printf("%s ASFDetectFaces fail: %ld\n", __func__, res);
        }
        else
        {
            SingleDetectedFaces.faceRect.left = detectedFaces.faceRect[0].left;
            SingleDetectedFaces.faceRect.top = detectedFaces.faceRect[0].top;
            SingleDetectedFaces.faceRect.right = detectedFaces.faceRect[0].right;
            SingleDetectedFaces.faceRect.bottom = detectedFaces.faceRect[0].bottom;
            SingleDetectedFaces.faceOrient = detectedFaces.faceOrient[0];

            // printf("Face Id: %d\n", detectedFaces.faceID[0]);
            // printf("Face Orient: %d\n", SingleDetectedFaces.faceOrient);
            printf("Face Rect: (%d %d %d %d)\n",
                   SingleDetectedFaces.faceRect.left, SingleDetectedFaces.faceRect.top,
                   SingleDetectedFaces.faceRect.right,
                   SingleDetectedFaces.faceRect.bottom);
            Mat imgBGR;
            cvtColor(yuv420p, imgBGR, COLOR_YUV2BGR_I420);
            rectangle(imgBGR, Point(SingleDetectedFaces.faceRect.left, SingleDetectedFaces.faceRect.top),
                      Point(SingleDetectedFaces.faceRect.right, SingleDetectedFaces.faceRect.bottom), Scalar(0, 0, 255), 1);
            mat_queue->push(imgBGR);
        }
    }
    return nullptr;
}

void *show_face_result(void *arg)
{
    CMatQueue *mat_queue = CMatQueue::getInstance();
    Mat img;
    int key;
    while (!stop_flag.load())
    {
        mat_queue->pop(img);
        if (!img.empty())
        {
            imshow("Face Rect", img);
            key = waitKey(1);  // 处理窗口事件
            if (key == 27)  // 按下 'Esc' 键退出
            {
                stop_flag.store(true);
                break;
            }
        }
    }
    return nullptr;
}

int main()
{
    image_yuv420p_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, VIDEO_WIDTH, VIDEO_HEIGHT, 1);

    pthread_t camera_thread = 0;
    pthread_t face_recognition_thread = 0;
    pthread_t show_result_thread = 0;
    pthread_create(&camera_thread, NULL, get_camera_data, NULL);
    pthread_create(&face_recognition_thread, NULL, face_recognition, NULL);

    pthread_create(&show_result_thread, NULL, show_face_result, NULL);

    pthread_join(camera_thread, NULL);
    pthread_join(face_recognition_thread, NULL);
    pthread_join(show_result_thread, NULL);
    
    av_fifo_freep(&m_videoFifo);
    
    printf("exit\n");
    return 0;
}
