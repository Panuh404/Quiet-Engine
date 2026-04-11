#include "Quiet/Core/UUID.h"

#include <array>
#include <charconv>
#include <random>
#include <sstream>

namespace Quiet::Core
{
    UUID UUIDGenerator::New()
    {
        static thread_local std::mt19937_64 rng(std::random_device{}());

        UUID id{};
        id.High = rng();
        id.Low = rng();

        if (!id.IsValid())
        {
            id.Low = 1;
        }

        return id;
    }

    std::string UUIDToString(const UUID &id)
    {
        std::ostringstream oss;
        oss << std::hex;
        oss.width(16);
        oss.fill('0');
        oss << id.High;
        oss << "-";
        oss.width(16);
        oss.fill('0');
        oss << id.Low;
        return oss.str();
    }

    bool UUIDFromString(const std::string &text, UUID &outId)
    {
        const auto separator = text.find('-');
        if (separator == std::string::npos)
            return false;

        const std::string highPart = text.substr(0, separator);
        const std::string lowPart = text.substr(separator + 1);

        std::uint64_t high = 0;
        std::uint64_t low = 0;

        const auto highResult = std::from_chars(
            highPart.data(),
            highPart.data() + highPart.size(),
            high,
            16
        );

        const auto lowResult = std::from_chars(
            lowPart.data(),
            lowPart.data() + lowPart.size(),
            low,
            16
        );

        if (highResult.ec != std::errc{} || lowResult.ec != std::errc{})
            return false;

        outId.High = high;
        outId.Low = low;
        return true;
    }

}
