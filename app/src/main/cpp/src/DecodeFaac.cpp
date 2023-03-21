#include <jni.h>
#include "AudioDecoder.h"
#include "fdk-aac/FDK_audio.h"
//
// Created by 127736 on 2023/3/21.
//


extern "C"
JNIEXPORT jstring JNICALL
Java_com_airplay_aac_MainActivity_stringFromJNI(JNIEnv *env, jobject thiz) {
    // TODO: implement stringFromJNI()
    PacketQueue *packetQueue = new PacketQueue();
    AudioDecoder *audioDecoder = new AudioDecoder(packetQueue);
    audioDecoder->open(48000, 1, AOT_ER_AAC_ELD);
}