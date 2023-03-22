//
// Created by liuhongwei on 2021/12/7.
//

#include <unistd.h>
#include "AudioDecoder.h"

#include <jni.h>
#include <pthread.h>
#include "Log.h"

// 判断编码器是否支持某个采样格式(采样大小)
static int check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt)
{
    const enum AVSampleFormat *p = codec->sample_fmts;

    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p == sample_fmt)
            return 1;
        p++;
    }
    return 0;
}

AudioDecoder::AudioDecoder(PacketQueue *packetQueue) {
    pPacketQueue = packetQueue;
    pFrameDataCallbackMutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    int ret = pthread_mutex_init(pFrameDataCallbackMutex, nullptr);
    if (ret != 0) {
        ALOGE("audio FrameDataCallbackMutex init failed.\n");
    }

    pFrameDataCallback = nullptr;
    pSwrContext = nullptr;
    pPCM16OutBuf = nullptr;
}

AudioDecoder::~AudioDecoder() {
    pthread_mutex_destroy(pFrameDataCallbackMutex);

    if (nullptr != pFrameDataCallbackMutex) {
        free(pFrameDataCallbackMutex);
        pFrameDataCallbackMutex = nullptr;
    }
}

bool AudioDecoder::open(unsigned int sampleFreq, unsigned int channels, unsigned int profile) {
    gSampleFreq = sampleFreq;

    int ret;
    AVCodec *dec = avcodec_find_decoder_by_name("libfdk_aac");
    ALOGI("%s audio decoder name: %s", __FUNCTION__, dec->name);
    enum AVSampleFormat sample_fmt = AV_SAMPLE_FMT_S16;//注意：设置为其他值并不生效
    int bytesPerSample = av_get_bytes_per_sample(sample_fmt);

    pAudioAVCodecCtx = avcodec_alloc_context3(dec);

    if (pAudioAVCodecCtx == nullptr) {
        ALOGE("%s AudioAVCodecCtx alloc failed", __FUNCTION__);
        return false;
    }

//    AVCodecParameters *par = avcodec_parameters_alloc();
//    if (par == nullptr) {
//        ALOGE("%s audio AVCodecParameters alloc failed", __FUNCTION__);
//        avcodec_free_context(&pAudioAVCodecCtx);
//        return false;
//    }
//    avcodec_parameters_to_context(pAudioAVCodecCtx, par);
//    avcodec_parameters_free(&par);


    pAudioAVCodecCtx->sample_rate    = (int) sampleFreq;
    // channel_layout为各个通道存储顺序，可以据此算出声道数。设置声道数也可以直接写具体值
    pAudioAVCodecCtx->channel_layout = av_get_default_channel_layout((int) channels);
    pAudioAVCodecCtx->channels       = (int) channels;
    pAudioAVCodecCtx->profile = FF_PROFILE_AAC_ELD;
    pAudioAVCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;

    ALOGI("%s sample_rate=%d channels=%d bytesPerSample=%d", __FUNCTION__, sampleFreq, channels,
         bytesPerSample);
    ret = avcodec_open2(pAudioAVCodecCtx, dec, nullptr);
    if (ret < 0) {
        ALOGE("%s Can not open audio encoder", __FUNCTION__);
        avcodec_free_context(&pAudioAVCodecCtx);
        return false;
    }
    ALOGI("%s avcodec_open2 audio SUCC", __FUNCTION__);
    pFrame = av_frame_alloc();
    if (pFrame == nullptr) {
        ALOGE("%s audio av_frame_alloc failed", __FUNCTION__);
        avcodec_free_context(&pAudioAVCodecCtx);
        return false;
    }

    pSwrContext = swr_alloc();
    if (pSwrContext == nullptr) {
        ALOGE("%s swr_alloc failed", __FUNCTION__);
        avcodec_free_context(&pAudioAVCodecCtx);
        av_frame_free(&pFrame);
        return false;
    }

    swr_alloc_set_opts(
            pSwrContext,
            pAudioAVCodecCtx->channel_layout,
            pAudioAVCodecCtx->sample_fmt,
            pAudioAVCodecCtx->sample_rate,
            pAudioAVCodecCtx->channel_layout,
            pAudioAVCodecCtx->sample_fmt,
            pAudioAVCodecCtx->sample_rate,
            0, nullptr
    );

    ret = swr_init(pSwrContext);
    if (ret != 0) {
        ALOGE("%s swr_init failed %d ", __FUNCTION__, ret);
        avcodec_free_context(&pAudioAVCodecCtx);
        av_frame_free(&pFrame);
        swr_free(&pSwrContext);
        return false;
    }

    pPCM16OutBuf = (uint8_t *) malloc(
            av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * 1024);

    if (pPCM16OutBuf == nullptr) {
        ALOGE("%s PCM16OutBufs malloc failed", __FUNCTION__);
        avcodec_free_context(&pAudioAVCodecCtx);
        av_frame_free(&pFrame);
        swr_free(&pSwrContext);
        return false;
    }

    isDecoding = true;
    ret = pthread_create(&decodeThread, nullptr, &AudioDecoder::_decode, (void *) this);
    if (ret != 0) {
        ALOGE("audio decode-thread create failed.\n");
        isDecoding = false;
        avcodec_free_context(&pAudioAVCodecCtx);
        av_frame_free(&pFrame);
        swr_free(&pSwrContext);

        free(pPCM16OutBuf);
        pPCM16OutBuf = nullptr;
        return false;
    }

    return true;
}

void AudioDecoder::close() {
    isDecoding = false;
    pthread_join(decodeThread, nullptr);

    if (pPCM16OutBuf != nullptr) {
        free(pPCM16OutBuf);
        pPCM16OutBuf = nullptr;
        ALOGI("%s PCM16OutBuf free", __FUNCTION__);
    }

    if (pSwrContext != nullptr) {
        swr_free(&pSwrContext);
        ALOGI("%s SwrContext free", __FUNCTION__);
    }

    if (pFrame != nullptr) {
        av_frame_free(&pFrame);
        ALOGI("%s audio Frame free", __FUNCTION__);
    }

    if (pAudioAVCodecCtx != nullptr) {
        avcodec_free_context(&pAudioAVCodecCtx);
        ALOGI("%s audio avcodec_free_context", __FUNCTION__);
    }
}

void AudioDecoder::setFrameDataCallback(FrameDataCallback *frameDataCallback) {
    pthread_mutex_lock(pFrameDataCallbackMutex);
    pFrameDataCallback = frameDataCallback;
    pthread_mutex_unlock(pFrameDataCallbackMutex);
}

void AudioDecoder::decode() {
    int ret;
    unsigned sleepDelta = 1024 * 1000000 / gSampleFreq / 4;// 一帧音频的 1/4

    while (isDecoding) {
        if (pPacketQueue == nullptr) {
            usleep(sleepDelta);
            continue;
        }
        ALOGE("%s 111=%d", __FUNCTION__, ret);

        AVPacket *pkt = av_packet_alloc();
        if (pkt == nullptr) {
            usleep(sleepDelta);
            continue;
        }
        ALOGE("%s 222=%d", __FUNCTION__, ret);

        Packet *packetStruct = pPacketQueue->getPacket();
        if (packetStruct != nullptr && packetStruct->data != nullptr &&
            packetStruct->data_size > 0) {

            ALOGE("%s 333=%d", __FUNCTION__, ret);


            ret = av_new_packet(pkt, packetStruct->data_size);
            if (ret < 0) {
                av_packet_free(&pkt);
                free(packetStruct->data);
                free(packetStruct);

                continue;
            }
        } else {
            av_packet_free(&pkt);
            usleep(sleepDelta);
            continue;
        }

        memcpy(pkt->data, packetStruct->data, packetStruct->data_size);

        pkt->pts = packetStruct->timestamp;
        pkt->dts = packetStruct->timestamp;


        /* send the packet for decoding */
        ret = avcodec_send_packet(pAudioAVCodecCtx, pkt);
        //LOGD("%s send the audio packet for decoding pkt size=%d", __FUNCTION__, pkt->size);
        free(packetStruct->data);
        free(packetStruct);

        av_packet_unref(pkt);
        av_packet_free(&pkt);

        if (ret < 0) {
            ALOGE("%s Error sending the audio pkt to the decoder ret=%d", __FUNCTION__, ret);
            usleep(sleepDelta);
            continue;
        } else {
            // 编码和解码都是一样的，都是send 1次，然后receive多次, 直到AVERROR(EAGAIN)或者AVERROR_EOF
            while (ret >= 0) {
                ret = avcodec_receive_frame(pAudioAVCodecCtx, pFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    usleep(sleepDelta);
                    continue;
                } else if (ret < 0) {
                    ALOGE("%s Error receive decoding audio frame ret=%d", __FUNCTION__, ret);
                    usleep(sleepDelta);
                    continue;
                }

                // 解码固定为 AV_SAMPLE_FMT_FLTP，需要转码为 AV_SAMPLE_FMT_S16
                // 数据都装在 data[0] 中，而大小则为 linesize[0]（实际发现此处大小并不对，大小计算见下面）
                int planeNum = 1;
                int dataLen[planeNum];
                /*dataLen[0] = pFrame->nb_samples *
                             av_get_bytes_per_sample((enum AVSampleFormat) (pFrame->format));*/
                // 重采样转为 S16
                uint8_t *pcmOut[1] = {nullptr};
                pcmOut[0] = pPCM16OutBuf;
                // 音频重采样
                int number = swr_convert(
                        pSwrContext,
                        pcmOut,
                        pFrame->nb_samples,
                        (const uint8_t **) pFrame->data,
                        pFrame->nb_samples
                );

                if (number != pFrame->nb_samples) {
                    ALOGE("%s swr_convert appear problem number=%d", __FUNCTION__, number);
                } else {
                    dataLen[0] = pFrame->nb_samples *
                                 av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
                    pthread_mutex_lock(pFrameDataCallbackMutex);
                    if (pFrameDataCallback != nullptr) {
                        ALOGD("%s receive the decode frame size=%d nb_samples=%d", __FUNCTION__, dataLen[0], pFrame->nb_samples);
                        pFrameDataCallback->onDataArrived((long long) pFrame->pts,
                                                          (char **) pcmOut,
                                                          dataLen,
                                                          planeNum,
                                                          pAudioAVCodecCtx->channels,
                                                          pAudioAVCodecCtx->sample_rate,
                                                          -1,
                                                          -1);
                    }
                    pthread_mutex_unlock(pFrameDataCallbackMutex);

                }


                av_frame_unref(pFrame);
            }
        }

    }
}

