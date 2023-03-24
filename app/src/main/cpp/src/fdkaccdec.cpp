/*
 * decode AAC-ELD audio data from mac by XBMC, and play it by SDL
 *
 * modify:
 * 2012-10-31   first version (ffmpeg tutorial03.c)
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include <fdk-aac/aacdecoder_lib.h>
#include <cstring>
#include "fdk-aac/FDK_audio.h"
#include "Log.h"
#include <jni.h>

#define LOG_TAG "aaa"

/* ---------------------------------------------------------- */
/*          enable file save, test pcm source                 */
/* ---------------------------------------------------------- */
#define ENABLE_PCM_SAVE

#ifdef ENABLE_PCM_SAVE
FILE *pout = NULL;
#endif
/* ---------------------------------------------------------- */
/*          next n lines is libfdk-aac config                 */
/* ---------------------------------------------------------- */
static int fdk_flags = 0;

/* period size 480 samples */
#define N_SAMPLE 480
/* ASC config binary data */
UCHAR eld_conf[] = { 0xF8, 0xE8, 0x50, 0x00 };
UCHAR *conf[] = { eld_conf };                   //TODO just for aac eld config
static UINT conf_len = sizeof(eld_conf);

static HANDLE_AACDECODER phandle = NULL;
static TRANSPORT_TYPE transportFmt = TT_MP4_RAW;         //raw data format
static UINT nrOfLayers = 1;                     //only one layer here
static CStreamInfo *aac_stream_info = NULL;

static int pcm_pkt_size = 4 * N_SAMPLE;

/*
 * decoding AAC format audio data by libfdk_aac
 */
int fdk_decode_audio(INT_PCM *output_buf, int *output_size, uint8_t *buffer, int size)
{
    int ret = 0;
    UINT pkt_size = size;
    UCHAR *input_buf[1] = {buffer};

    /* step 1 -> fill aac_data_buf to decoder's internal buf */
    ret = aacDecoder_Fill(phandle, input_buf, &pkt_size, &pkt_size);
    if (ret != AAC_DEC_OK) {
        ALOGD("Fill failed: %x\n", ret);
        *output_size  = 0;
        return 0;
    }

    /* step 2 -> call decoder function */
    ret = aacDecoder_DecodeFrame(phandle, output_buf, pcm_pkt_size, fdk_flags);
    if (ret == AAC_DEC_NOT_ENOUGH_BITS) {
        ALOGD("not enough\n");
        *output_size  = 0;
    }
    if (ret != AAC_DEC_OK) {
        ALOGD("aacDecoder_DecodeFrame : 0x%x\n", ret);
        *output_size  = 0;
        return 0;
    }
    ALOGD("AAC_DEC_OK_NUM : %d", pcm_pkt_size);
    *output_size = pcm_pkt_size;

#ifdef ENABLE_PCM_SAVE
    fwrite((uint8_t *)output_buf, 1, pcm_pkt_size, pout);
#endif
    /* return aac decode size */
    return 1;
}

#define RAOP_BUFFER_LENGTH 512

void audio_decode_frame(uint8_t *audio_buf, int buf_size)
{
    ALOGD("audio_decode_frame buf_size: %d \n", buf_size);
    int audio_buffer_size = 480 * 2 * 2;

    int buffer_size = audio_buffer_size * RAOP_BUFFER_LENGTH;
    void *buffer = malloc(buffer_size);
    INT_PCM *output_buf = (INT_PCM *)(buffer);
    fdk_decode_audio(output_buf, &buffer_size, audio_buf, buf_size);
    free(buffer);
}

/*
 * init fdk decoder
 */
void init_fdk_decoder()
{
    int ret = 0;

    phandle = aacDecoder_Open(transportFmt, nrOfLayers);
    if (phandle == nullptr) {
        ALOGD("aacDecoder open faild!\n");
        return;
    }

    printf("conf_len = %d\n", conf_len);
    ret = aacDecoder_ConfigRaw(phandle, conf, &conf_len);
    if (ret != AAC_DEC_OK) {
        ALOGD("Unable to set configRaw\n");
        return;
    }

    aac_stream_info = aacDecoder_GetStreamInfo(phandle);
    if (aac_stream_info == nullptr) {
        ALOGD("aacDecoder_GetStreamInfo failed!\n");
        return;
    }
    ALOGD("stream info: channel = %d sample_rate = %d tframe_size = %d taot = %d tbitrate = %d\n",
    aac_stream_info->channelConfig,
        aac_stream_info->aacSampleRate,
            aac_stream_info->aacSamplesPerFrame,
                aac_stream_info->aot,
                    aac_stream_info->bitRate);
}

/*
 * first init func, called by external
 */
void init_fdk_aac_decode()
{
    /* init fdk decoder */
    init_fdk_decoder();

#ifdef ENABLE_PCM_SAVE
    pout = fopen("/data/data/com.airplay.aac/cache/star.pcm", "wb");
    if (pout == nullptr) {
        ALOGD("open star.pcm file failed!\n");
        return;
    }
#endif

}

extern "C"
JNIEXPORT void JNICALL
Java_com_airplay_aac_MainActivity_initFdk(JNIEnv *env, jobject thiz) {
    init_fdk_aac_decode();
}
extern "C"
JNIEXPORT void JNICALL
Java_com_airplay_aac_MainActivity_decodeFdk(JNIEnv *env, jobject thiz, jbyteArray buffer, jint num) {
    uint8_t* data = (uint8_t*)env->GetByteArrayElements(buffer, 0);
    audio_decode_frame(data, num);
}