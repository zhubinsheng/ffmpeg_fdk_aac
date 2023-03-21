//
// Created by liuhongwei on 2021/12/7.
//

#ifndef AUDIODECODER_H
#define AUDIODECODER_H

extern "C" {
//编解码
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libavutil/frame.h"
#include "libavutil/mem.h"

#include <jni.h>
}


class FrameDataCallback {
public:
	virtual ~FrameDataCallback() { }
	virtual void onDataArrived() = 0;
};

typedef struct PacketQueue {

} PacketQueue;

class AudioDecoder {
public:
	AudioDecoder(PacketQueue *packetQueue);

	~AudioDecoder();

	bool open(unsigned int sampleFreq, unsigned int channels, unsigned int profile = 1);

	void close();

	void decode();

	static void *_decode(void *self) {
		static_cast<AudioDecoder *>(self)->decode();
		return nullptr;
	}

	void setFrameDataCallback(FrameDataCallback *frameDataCallback);

private:
	PacketQueue *pPacketQueue;
	AVCodecContext *pAudioAVCodecCtx;
	AVFrame *pFrame;
	unsigned int gSampleFreq;

	bool volatile isDecoding;
	pthread_t decodeThread;
	pthread_mutex_t *pFrameDataCallbackMutex;
	FrameDataCallback *pFrameDataCallback;

	SwrContext *pSwrContext;
	uint8_t *pPCM16OutBuf;
};


#endif //AUDIODECODER_H
