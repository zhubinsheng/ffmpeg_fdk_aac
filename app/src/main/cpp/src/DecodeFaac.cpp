#include <jni.h>
#include "AudioDecoder.h"
#include "fdk-aac/FDK_audio.h"
//
// Created by 127736 on 2023/3/21.
//
class FrameDataCallbackImpl : public FrameDataCallback{
public:
     void onDataArrived(long long int i, char **pString, int pInt[1], int i1, int i2, int i3, int i4, int i5) {

     };
};

PacketQueue *packetQueue = nullptr;
FrameDataCallbackImpl *frameDataCallback = nullptr;

extern "C"
JNIEXPORT void JNICALL
Java_com_airplay_aac_MainActivity_initDecoder(JNIEnv *env, jobject thiz) {
    packetQueue = new PacketQueue();
    auto *audioDecoder = new AudioDecoder(packetQueue);
//    audioDecoder->test();
    audioDecoder->open(48000, 1, AOT_ER_AAC_ELD);
//    frameDataCallback = new FrameDataCallbackImpl();
//    audioDecoder->setFrameDataCallback(frameDataCallback);

    //todo ~
}
extern "C"
JNIEXPORT void JNICALL
Java_com_airplay_aac_MainActivity_addPacket(JNIEnv *env, jobject thiz, jbyteArray buffer) {
    uint8_t* data = (uint8_t*)env->GetByteArrayElements(buffer, 0);
    jsize theArrayLengthJ = env->GetArrayLength(buffer);

    Packet *packet = new Packet();
    packet->data = data;
    packet->data_size = theArrayLengthJ;
    packetQueue->addPacket(packet);
}