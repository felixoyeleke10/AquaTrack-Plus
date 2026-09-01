#pragma once

#include <algorithm>
#include <string>

namespace aquatrack {

struct BoundingBox {
    float x1 = 0.0F;
    float y1 = 0.0F;
    float x2 = 0.0F;
    float y2 = 0.0F;

    [[nodiscard]] float width() const { return std::max(0.0F, x2 - x1); }
    [[nodiscard]] float height() const { return std::max(0.0F, y2 - y1); }
    [[nodiscard]] float area() const { return width() * height(); }
    [[nodiscard]] float centerX() const { return (x1 + x2) * 0.5F; }
    [[nodiscard]] float centerY() const { return (y1 + y2) * 0.5F; }
};

struct Detection {
    BoundingBox box;
    float confidence = 0.0F;
    int classId = 0;
    std::string className;
};

struct Track {
    int id = 0;
    Detection detection;
    int hits = 1;
    int missed = 0;
    bool confirmed = false;
};

}  // namespace aquatrack
