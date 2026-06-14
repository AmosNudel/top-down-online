#ifndef NET_FRAMING_H
#define NET_FRAMING_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace NetFraming
{
    // Stream frame: [uint16_be length][payload]
    constexpr uint16_t kMaxPayload = 60000;

    inline uint16_t readU16Be(const uint8_t *data)
    {
        return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
    }

    inline void writeU16Be(uint16_t value, std::vector<uint8_t> &out)
    {
        out.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
        out.push_back(static_cast<uint8_t>(value & 0xff));
    }

    inline void encodeFrame(const uint8_t *data, size_t size, std::vector<uint8_t> &out)
    {
        if (size > kMaxPayload)
            return;
        writeU16Be(static_cast<uint16_t>(size), out);
        out.insert(out.end(), data, data + size);
    }

    // Append incoming TCP bytes; returns complete payloads in `packets`.
    inline void feed(
        std::vector<uint8_t> &recvBuffer,
        const uint8_t *data,
        size_t size,
        std::vector<std::vector<uint8_t>> &packets)
    {
        if (!data || size == 0)
            return;

        recvBuffer.insert(recvBuffer.end(), data, data + size);

        while (recvBuffer.size() >= 2)
        {
            const uint16_t payloadLen = readU16Be(recvBuffer.data());
            if (payloadLen > kMaxPayload)
            {
                recvBuffer.clear();
                break;
            }

            const size_t frameSize = 2u + static_cast<size_t>(payloadLen);
            if (recvBuffer.size() < frameSize)
                break;

            packets.emplace_back(recvBuffer.begin() + 2, recvBuffer.begin() + frameSize);
            recvBuffer.erase(recvBuffer.begin(), recvBuffer.begin() + static_cast<std::ptrdiff_t>(frameSize));
        }
    }
}

#endif // NET_FRAMING_H
