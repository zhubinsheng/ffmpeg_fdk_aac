#include "PacketQueue.h"
#include "Log.h"

PacketQueue::PacketQueue()
{
}

void PacketQueue::addPacket(Packet * aPacket)
{
    ALOGI("adding packet to queue", __FUNCTION__);
    std::lock_guard<std::recursive_mutex> lg(mutex);
    queue.push(aPacket);
    ALOGI("packet added", __FUNCTION__);
}

bool PacketQueue::empty()
{
    std::lock_guard<std::recursive_mutex> lg(mutex);
    return queue.empty();
}

Packet * PacketQueue::getPacket() //Gives you back the first packet in the queue and destroys it
{
    std::lock_guard<std::recursive_mutex> lg(mutex);

    if (queue.empty()){
        ALOGE("%s queue empty... ", __FUNCTION__);
        return nullptr;
    }
    Packet *packetStruct = queue.front();
    queue.pop();

    return packetStruct;
}

std::recursive_mutex& PacketQueue::getMutex()
{
    return mutex;
}
