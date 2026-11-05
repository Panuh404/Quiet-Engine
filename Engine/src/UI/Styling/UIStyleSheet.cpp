#include "Quiet/UI/Styling/UIStyleSheet.h"

#include <array>
#include <cmath>
#include <unordered_set>
#include <utility>
#include <stdexcept>
#include <fstream>

#include <nlohmann/json.hpp>

namespace Quiet::UI
{
    bool ValidateUIStyleSheet(const UIStyleSheet& sheet, std::string& outError)
    {
        outError.clear();
        std::unordered_set<Core::UUID> ids;

        const auto finiteColor = [](const glm::vec4& color)
        {
            return std::isfinite(color.r) &&
                   std::isfinite(color.g) &&
                   std::isfinite(color.b) &&
                   std::isfinite(color.a);
        };

        for (const UIStyleEntry& entry : sheet.Styles)
        {
            if (!entry.ID.IsValid())
            {
                outError = "UI style has an invalid UUID.";
                return false;
            }

            if (!ids.insert(entry.ID).second)
            {
                outError = "UI style has a duplicate UUID: " + Core::UUIDToString(entry.ID);
                return false;
            }

            if (entry.Properties.valueless_by_exception())
            {
                outError = "UI style has no properties: " + Core::UUIDToString(entry.ID);
                return false;
            }

            if (const auto* style = std::get_if<ButtonStyle>(&entry.Properties))
            {
                const glm::vec4& padding = style->Padding;

                if (!std::isfinite(padding.x) ||
                    !std::isfinite(padding.y) ||
                    !std::isfinite(padding.z) ||
                    !std::isfinite(padding.w) ||
                    padding.x < 0.0f ||
                    padding.y < 0.0f ||
                    padding.z < 0.0f ||
                    padding.w < 0.0f)
                {
                    outError = "Invalid button padding in UI style: " + Core::UUIDToString(entry.ID);
                    return false;
                }

                const std::array colors
                {
                    &style->NormalColor,
                    &style->HoveredColor,
                    &style->PressedColor,
                    &style->DisabledColor,
                    &style->FocusedColor
                };

                for (const glm::vec4* color : colors)
                {
                    if (!finiteColor(*color))
                    {
                        outError = "Invalid button color in UI style: " + Core::UUIDToString(entry.ID);
                        return false;
                    }
                }
            }

            if (const auto* style = std::get_if<TextStyle>(&entry.Properties))
            {
                if (!std::isfinite(style->PixelHeight) ||
                    style->PixelHeight <= 0.0f ||
                    !finiteColor(style->Color))
                {
                    outError = "Invalid text style: " + Core::UUIDToString(entry.ID);
                    return false;
                }
            }
        }
        return true;
    }

    bool SerializeUIStyleSheet(const UIStyleSheet& sheet, std::string& outJSON, std::string& outError)
    {
        if (!ValidateUIStyleSheet(sheet, outError)) return false;

        using json = nlohmann::json;
        const auto writeVec4 = [](const glm::vec4& value)
        {
            return json::array({value.x, value.y, value.z, value.w});
        };

        try
        {
            json root;
            root["version"] = 1;
            root["styles"] = json::array();

            for (const UIStyleEntry& entry : sheet.Styles)
            {
                json value;
                value["id"] = Core::UUIDToString(entry.ID);
                value["name"] = entry.Name;

                json properties = json::object();

                if (const auto* style = std::get_if<ButtonStyle>(&entry.Properties))
                {
                    value["type"] = "button";

                    properties["normal_color"] = writeVec4(style->NormalColor);
                    properties["hovered_color"] = writeVec4(style->HoveredColor);
                    properties["pressed_color"] = writeVec4(style->PressedColor);
                    properties["disabled_color"] = writeVec4(style->DisabledColor);
                    properties["focused_color"] = writeVec4(style->FocusedColor);
                    properties["padding"] = writeVec4(style->Padding);
                }
                else if (const auto* style = std::get_if<TextStyle>(&entry.Properties))
                {
                    value["type"] = "text";
                    properties["pixel_height"] = style->PixelHeight;
                    properties["color"] = writeVec4(style->Color);
                }

                value["properties"] = std::move(properties);
                root["styles"].push_back(std::move(value));
            }

            std::string serialized = root.dump(4);
            outJSON = std::move(serialized);
            return true;
        }
        catch (const json::exception& exception)
        {
            outError = std::string("Failed to serialize UI style sheet: ") + exception.what();
            return false;
        }
    }

    bool DeserializeUIStyleSheet(const std::string& jsonText, UIStyleSheet& outSheet, std::string& outError)
    {
        using json = nlohmann::json;

        outError.clear();

        try
        {
            const json root = json::parse(jsonText);

            if (!root.is_object() ||
                !root.at("version").is_number_integer() ||
                root.at("version") != 1)
            {
                throw std::runtime_error("Unsupported UI style sheet version.");
            }

            const json& styles = root.at("styles");

            if (!styles.is_array())
            {
                throw std::runtime_error("UI styles must be an array.");
            }

            const auto readVec4 = [](const json& value) -> glm::vec4
            {
                if (!value.is_array() || value.size() != 4)
                {
                    throw std::runtime_error("Expected a four-component UI style vector.");
                }

                return {
                    value.at(0).get<float>(),
                    value.at(1).get<float>(),
                    value.at(2).get<float>(),
                    value.at(3).get<float>()
                };
            };

            UIStyleSheet sheet;

            for (const json& value : styles)
            {
                if (!value.is_object())
                {
                    throw std::runtime_error("UI style must be an object.");
                }

                UIStyleEntry entry;

                const std::string idText = value.at("id").get<std::string>();

                if (!Core::UUIDFromString(idText, entry.ID) || !entry.ID.IsValid())
                {
                    throw std::runtime_error("Invalid UI style UUID: " + idText);
                }

                entry.Name = value.at("name").get<std::string>();

                const std::string type = value.at("type").get<std::string>();
                const json& properties = value.at("properties");

                if (!properties.is_object())
                {
                    throw std::runtime_error(
                        "UI style properties must be an object.");
                }

                if (type == "button")
                {
                    ButtonStyle style;
                    style.NormalColor = readVec4(properties.at("normal_color"));
                    style.HoveredColor = readVec4(properties.at("hovered_color"));
                    style.PressedColor = readVec4(properties.at("pressed_color"));
                    style.DisabledColor = readVec4(properties.at("disabled_color"));
                    style.FocusedColor = readVec4(properties.at("focused_color"));
                    style.Padding = readVec4(properties.at("padding"));

                    entry.Properties = style;
                }
                else if (type == "text")
                {
                    TextStyle style;
                    style.PixelHeight = properties.at("pixel_height").get<float>();
                    style.Color = readVec4(properties.at("color"));

                    entry.Properties = style;
                }
                else
                {
                    throw std::runtime_error("Unknown UI style type: " + type);
                }

                sheet.Styles.push_back(std::move(entry));
            }

            if (!ValidateUIStyleSheet(sheet, outError))
            {
                return false;
            }

            outSheet = std::move(sheet);
            return true;
        }
        catch (const json::exception& exception)
        {
            outError = std::string("Failed to read UI style sheet JSON: ") + exception.what();
            return false;
        }
        catch (const std::runtime_error& exception)
        {
            outError = std::string("Invalid UI style sheet: ") + exception.what();
            return false;
        }
    }

    bool SaveUIStyleSheetToFile(const std::filesystem::path& path, const UIStyleSheet& sheet, std::string& outError)
    {
        std::string jsonText;
        if (!SerializeUIStyleSheet(sheet, jsonText, outError))
        {
            return false;
        }

        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            outError = "Failed to open UI style sheet for writing: " + path.string();
            return false;
        }

        output << jsonText;
        output.close();
        if (!output)
        {
            outError = "Failed to write UI style sheet: " + path.string();
            return false;
        }
        return true;
    }

    bool LoadUIStyleSheetFromFile(const std::filesystem::path& path, UIStyleSheet& outSheet, std::string& outError)
    {
        outError.clear();

        std::ifstream input(path, std::ios::binary);
        if (!input.is_open())
        {
            outError = "Failed to open UI style sheet for reading: " + path.string();
            return false;
        }

        std::string jsonText;
        std::array<char, 4096> buffer{};

        while (input.read(buffer.data(), buffer.size()) || input.gcount() > 0)
        {
            jsonText.append(buffer.data(), static_cast<std::size_t>(input.gcount()));
        }

        if (input.bad() || (!input.eof() && input.fail()))
        {
            outError = "Failed to read UI style sheet: " + path.string();
            return false;
        }

        return DeserializeUIStyleSheet(jsonText, outSheet, outError);
    }
}
