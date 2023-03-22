//
// Created by liuhongwei on 2021/12/7.
//

#include <unistd.h>
#include "AudioDecoder.h"

#include <jni.h>
#include <pthread.h>
#include "Log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        if (packetStruct != nullptr &&
                packetStruct->data != nullptr &&
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

#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096

static int get_format_from_sample_fmt(const char **fmt,
                                      enum AVSampleFormat sample_fmt)
{
    int i;
    struct sample_fmt_entry {
        enum AVSampleFormat sample_fmt; const char *fmt_be, *fmt_le;
    } sample_fmt_entries[] = {
            { AV_SAMPLE_FMT_U8,  "u8",    "u8"    },
            { AV_SAMPLE_FMT_S16, "s16be", "s16le" },
            { AV_SAMPLE_FMT_S32, "s32be", "s32le" },
            { AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
            { AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
    };
    *fmt = NULL;

    for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
        struct sample_fmt_entry *entry = &sample_fmt_entries[i];
        if (sample_fmt == entry->sample_fmt) {
            *fmt = AV_NE(entry->fmt_be, entry->fmt_le);
            return 0;
        }
    }

    ALOGW(
            "sample format %s is not supported as output format\n",
            av_get_sample_fmt_name(sample_fmt));
    return -1;
}

static void decode2(AVCodecContext *dec_ctx, AVPacket *pkt, AVFrame *frame,
                   FILE *outfile)
{
    int i, ch;
    int ret, data_size;

    /* send the packet with the compressed data to the decoder */
    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        ALOGW( "Error submitting the packet to the decoder\n");
        exit(1);
    }

    /* read all the output frames (in general there may be any number of them */
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            ALOGW( "Error during decoding\n");
            exit(1);
        }
        data_size = av_get_bytes_per_sample(dec_ctx->sample_fmt);
        if (data_size < 0) {
            /* This should not occur, checking just for paranoia */
            ALOGW( "Failed to calculate data size\n");
            exit(1);
        }
        for (i = 0; i < frame->nb_samples; i++)
            for (ch = 0; ch < 1; ch++)
                fwrite(frame->data[ch] + data_size*i, 1, data_size, outfile);
    }
}

void AudioDecoder::test()
{
    const char *outfilename, *filename;
    const AVCodec *codec;
    AVCodecContext *c= NULL;
    AVCodecParserContext *parser = NULL;
    int len, ret;
    FILE *f, *outfile;
    uint8_t inbuf[AUDIO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    uint8_t *data;
    size_t   data_size;
    AVPacket *pkt;
    AVFrame *decoded_frame = NULL;
    enum AVSampleFormat sfmt;
    int n_channels = 0;
    const char *fmt;

    filename    = "/data/data/com.airplay.aac/cache/output.aac";
    outfilename = "/data/data/com.airplay.aac/cache/test.pcm";

    pkt = av_packet_alloc();

    /* find the MPEG audio decoder */
    codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
    if (!codec) {
        ALOGW( "Codec not found\n");
        exit(1);
    }

    parser = av_parser_init(codec->id);
    if (!parser) {
        ALOGW( "Parser not found\n");
        exit(1);
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        ALOGW( "Could not allocate audio codec context\n");
        exit(1);
    }

    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
        ALOGW( "Could not open codec\n");
        exit(1);
    }

    f = fopen(filename, "rb");
    if (!f) {
        ALOGW( "Could not open %s\n", filename);
        exit(1);
    }
    outfile = fopen(outfilename, "wb");
    if (!outfile) {
        av_free(c);
        exit(1);
    }

    /* decode until eof */
    data      = inbuf;
    data_size = fread(inbuf, 1, AUDIO_INBUF_SIZE, f);

    while (data_size > 0) {
        if (!decoded_frame) {
            if (!(decoded_frame = av_frame_alloc())) {
                ALOGW( "Could not allocate audio frame\n");
                exit(1);
            }
        }

        ret = av_parser_parse2(parser, c, &pkt->data, &pkt->size,
                               data, data_size,
                               AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        if (ret < 0) {
            ALOGW( "Error while parsing\n");
            exit(1);
        }
        data      += ret;
        data_size -= ret;
        ALOGW( "Error while parsing %d \n", c->channels);
        ALOGW( "Error while parsing %d \n", c->sample_fmt);

        if (pkt->size)
            decode2(c, pkt, decoded_frame, outfile);

        if (data_size < AUDIO_REFILL_THRESH) {
            memmove(inbuf, data, data_size);
            data = inbuf;
            len = fread(data + data_size, 1,
                        AUDIO_INBUF_SIZE - data_size, f);
            if (len > 0)
                data_size += len;
        }
    }

    /* flush the decoder */
    pkt->data = NULL;
    pkt->size = 0;
    decode2(c, pkt, decoded_frame, outfile);

    /* print output pcm infomations, because there have no metadata of pcm */
    sfmt = c->sample_fmt;

    if (av_sample_fmt_is_planar(sfmt)) {
        const char *packed = av_get_sample_fmt_name(sfmt);
        ALOGW("Warning: the sample format the decoder produced is planar "
               "(%s). This example will output the first channel only.\n",
               packed ? packed : "?");
        sfmt = av_get_packed_sample_fmt(sfmt);
    }

    n_channels = c->channels;
    if ((ret = get_format_from_sample_fmt(&fmt, sfmt)) < 0)
        goto end;

    ALOGW("Play the output audio file with the command:\n"
           "ffplay -f %s -ac %d -ar %d %s\n",
           fmt, n_channels, c->sample_rate,
           outfilename);
    end:
    fclose(outfile);
    fclose(f);

    avcodec_free_context(&c);
    av_parser_close(parser);
    av_frame_free(&decoded_frame);
    av_packet_free(&pkt);

    ALOGW("end end ");
}