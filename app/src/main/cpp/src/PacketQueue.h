/*
 *  Created by Roel Storms
 *
 *  This is a wrapper around an std::queue to provide concurrency safety.
 *  The queue has pointers to Packet objects. These queues are used to send packets between threads.
 *
 */

#ifndef PACKETQUEUE_H
#define PACKETQUEUE_H

#include <mutex>
#include <queue>
#include <condition_variable>

typedef struct Packet {
    uint8_t *data;
    int   data_size;
    int64_t timestamp;
} Packet;

class PacketQueue
{
private:
    std::recursive_mutex mutex;
    std::queue<Packet *> queue;

    PacketQueue(PacketQueue&);
    PacketQueue(const PacketQueue&);
    PacketQueue& operator=(const PacketQueue&);

public:
    PacketQueue();
    void addPacket(Packet * aPacket);
    bool empty();
    Packet * getPacket(); //Gives you back the first packet in the queue and destroys it
    std::recursive_mutex& getMutex();


};

#endif