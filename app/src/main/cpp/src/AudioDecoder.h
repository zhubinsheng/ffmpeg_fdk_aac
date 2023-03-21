//
// Created by liuhongwei on 2021/12/7.
//

#ifndef AUDIODECODER_H
#define AUDIODECODER_H

#include "PacketQueue.h"

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
	virtual void
    onDataArrived(long long int i, char **pString, int pInt[1], int i1, int i2, int i3,
                  int i4,
                  int i5) = 0;
};

class AudioDecoder {
public:
	AudioDecoder(PacketQueue *packetQueue);

	~AudioDecoder();

	bool open(unsigned int sampleFreq, unsigned int channels, unsigned int profile = 1);

	void close();

	void decode();

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

	static void *_decode(void *self) {
		static_cast<AudioDecoder *>(self)->decode();
		return nullptr;
	}
};


#endif //AUDIODECODER_H
