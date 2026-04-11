#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace Quiet::Core
{
    struct UUID
    {
        std::uint64_t High = 0;
        std::uint64_t Low = 0;

        [[nodiscard]] bool IsValid() const
        {
            return High != 0 || Low != 0;
        }

        auto operator<=>(const UUID&) const = default;
    };

    class UUIDGenerator
    {
    public:
        static UUID New();
    };

    [[nodiscard]] std::string UUIDToString(const UUID& id);
    [[nodiscard]] bool UUIDFromString(const std::string& text, UUID& outId);
}

namespace std
{
    template<>
    struct hash<Quiet::Core::UUID>
    {
        std::size_t operator()(const Quiet::Core::UUID& id) const noexcept
        {
            return std::hash<std::uint64_t>{}(id.High) ^
                   (std::hash<std::uint64_t>{}(id.Low) << 1);
        }
    };
}