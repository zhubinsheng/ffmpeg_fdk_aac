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


typedef unsigned char u8;
typedef unsigned short int u16;
typedef unsigned int u32;

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

/* ---------------------------------------------------------- */
/*          AAC data and queue list struct definition         */
/* ---------------------------------------------------------- */
static int quit = 0;

#define FDK_MAX_AUDIO_FRAME_SIZE    192000      //1 second of 48khz 32bit audio
#define SDL_AUDIO_BUFFER_SIZE 4 * N_SAMPLE
#define PCM_RATE        44100
#define PCM_CHANNEL     2

typedef struct AACPacket {
    unsigned char *data;
    unsigned int size;
} AACPacket;

typedef struct AACPacketList {
    AACPacket pkt;
    struct AACPacketList *next;
} AACPacketList;

typedef struct PacketQueue {
    AACPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;

} PacketQueue;

static PacketQueue audioq;
/* ---------------------------------------------------------- */
/*              local memcpy malloc                           */
/* ---------------------------------------------------------- */
/* for local memcpy malloc */
#define AAC_BUFFER_SIZE 1024 * 1024
#define THRESHOLD       1 * 1024

static u8 repo[AAC_BUFFER_SIZE] = {0};
static u8 *repo_ptr = NULL;
/*
 * init mem repo
 */
static void init_mem_repo(void)
{
    repo_ptr = repo;
}

/*
 * alloc input pkt buffer from input_aac_data[]
 */
static void *alloc_pkt_buf(void)
{
    int space;

    space = AAC_BUFFER_SIZE - (repo_ptr - repo);

    if (space < THRESHOLD) {
        repo_ptr = repo;
        return repo;
    }

    return repo_ptr;
}

static void set_pkt_size(int size)
{
    repo_ptr += size;
}
/* ---------------------------------------------------------- */

static int fdk_dup_packet(AACPacket *pkt)
{
    u8 *repo_ptr;

//    repo_ptr = alloc_pkt_buf();
    memcpy(repo_ptr, pkt->data, pkt->size);
    pkt->data = repo_ptr;

    set_pkt_size(pkt->size);

    return 0;
}

static int packet_queue_put(PacketQueue *q, AACPacket *pkt)
{
    //fprintf(stderr, "p");
    AACPacketList *pkt1;

    /* memcpy data from xbmc */
    fdk_dup_packet(pkt);

//    pkt1 = malloc(sizeof(AACPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;


    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size;


    return 0;
}

/*
 * called by external, aac data input queue
 */
int decode_copy_aac_data(u8 *data, int size)
{
    AACPacket pkt;

    pkt.data = data;
    pkt.size = size;

    packet_queue_put(&audioq, &pkt);

    return 0;
}

static int packet_queue_get(PacketQueue *q, AACPacket *pkt, int block)
{
    //fprintf(stderr, "g");
    AACPacketList *pkt1;
    int ret;

    for (;;) {
        if (quit) {
            ret = -1;
            break;
        }

        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size;
            *pkt = pkt1->pkt;
            free(pkt1);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        }
    }

    //fprintf(stderr, "o");
    return ret;
}

/*
 * decoding AAC format audio data by libfdk_aac
 */
int fdk_decode_audio(INT_PCM *output_buf, int *output_size, u8 *buffer, int size)
{
    int ret = 0;
    int pkt_size = size;
    UINT valid_size = size;
    UCHAR *input_buf[1] = {buffer};

    /* step 1 -> fill aac_data_buf to decoder's internal buf */
    ret = aacDecoder_Fill(phandle, input_buf, reinterpret_cast<const UINT *>(&pkt_size), &valid_size);
    if (ret != AAC_DEC_OK) {
        fprintf(stderr, "Fill failed: %x\n", ret);
        *output_size  = 0;
        return 0;
    }

    /* step 2 -> call decoder function */
    ret = aacDecoder_DecodeFrame(phandle, output_buf, pcm_pkt_size, fdk_flags);
    if (ret == AAC_DEC_NOT_ENOUGH_BITS) {
        fprintf(stderr, "not enough\n");
        *output_size  = 0;
        /*
         * TODO FIXME
         * if not enough, get more data
         *
         */
    }
    if (ret != AAC_DEC_OK) {
        fprintf(stderr, "aacDecoder_DecodeFrame : 0x%x\n", ret);
        *output_size  = 0;
        return 0;
    }

    *output_size = pcm_pkt_size;

#ifdef ENABLE_PCM_SAVE
    fwrite((u8 *)output_buf, 1, pcm_pkt_size, pout);
#endif
    /* return aac decode size */
    return (size - valid_size);
}

int audio_decode_frame(uint8_t *audio_buf, int buf_size)
{
    static AACPacket pkt;
    static uint8_t *audio_pkt_data = NULL;
    static int audio_pkt_size = 0;

    int len1, data_size;

    for (;;) {
        while (audio_pkt_size > 0) {
            data_size = buf_size;
            len1 = fdk_decode_audio((INT_PCM *)audio_buf, &data_size,
                                    audio_pkt_data, audio_pkt_size);
            if (len1 < 0) {
                /* if error, skip frame */
                audio_pkt_size = 0;
                break;
            }
            audio_pkt_data += len1;
            audio_pkt_size -= len1;
            if (data_size <= 0) {
                /* No data yet, get more frames */
                continue;
            }
            /* We have data, return it and come back for more later */
            //fprintf(stderr, "\ndata size = %d\n", data_size);
            return data_size;
        }
        /* FIXME
         * add by juguofeng
         * only no nead in this code, because we alloc a memcpy ourselves
         */
        //if(pkt.data)
        //  free(pkt.data);

        if (quit) {
            return -1;
        }

        if (packet_queue_get(&audioq, &pkt, 1) < 0) {
            return -1;
        }
        audio_pkt_data = pkt.data;
        audio_pkt_size = pkt.size;
    }
}

void audio_callback(void *userdata, uint8_t *stream, int len)
{
    int len1, audio_size;

    static uint8_t audio_buf[(FDK_MAX_AUDIO_FRAME_SIZE * 3) / 2];
    static unsigned int audio_buf_size = 0;
    static unsigned int audio_buf_index = 0;

    //fprintf(stderr, "callback len = %d\n", len);

    while (len > 0) {
        if (audio_buf_index >= audio_buf_size) {
            //fprintf(stderr, "c");
            /* We have already sent all our data; get more */
            audio_size = audio_decode_frame(audio_buf, sizeof(audio_buf));
            if (audio_size < 0) {
                /* If error, output silence */
                audio_buf_size = pcm_pkt_size;       // arbitrary?
                memset(audio_buf, 0, audio_buf_size);
            } else {
                audio_buf_size = audio_size;
            }
            audio_buf_index = 0;
        }
        len1 = audio_buf_size - audio_buf_index;
        if (len1 > len)
            len1 = len;
        memcpy(stream, (uint8_t *)audio_buf + audio_buf_index, len1);
        len -= len1;
        stream += len1;
        audio_buf_index += len1;
    }
}

#define LOG_TAG "aaa"
/*
 * init fdk decoder
 */
void init_fdk_decoder(void)
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
    init_mem_repo();

#ifdef ENABLE_PCM_SAVE
    pout = fopen("/data/data/com.airplay.aac/cache/star.pcm", "wb");
    if (pout == nullptr) {
        ALOGD("open star.pcm file failed!\n");
        return;
    }
#endif

//    packet_queue_init(&audioq);
//    SDL_PauseAudio(0);

    //packet_queue_put(&audioq, &packet);

}

extern "C"
JNIEXPORT void JNICALL
Java_com_airplay_aac_MainActivity_initFdk(JNIEnv *env, jobject thiz) {
    init_fdk_aac_decode();
}