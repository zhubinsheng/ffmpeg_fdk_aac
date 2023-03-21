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

Packet * PacketQueue::getPacket()
{
    std::lock_guard<std::recursive_mutex> lg(mutex);

    Packet * output = queue.front();
    queue.pop();
    return output;
}

std::recursive_mutex& PacketQueue::getMutex()
{
    return mutex;
}
