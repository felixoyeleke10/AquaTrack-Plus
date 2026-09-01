#include "domain/IoUTracker.h"
#include "TestSupport.h"

#include <cmath>

int main()
{
    using namespace aquatrack;
    const BoundingBox firstBox{0, 0, 10, 10};
    const BoundingBox secondBox{5, 5, 15, 15};
    requireTest(std::abs(intersectionOverUnion(firstBox, secondBox) - (25.0F / 175.0F)) < 0.0001F,
                "intersection over union");
    IoUTracker tracker(0.2F, 2, 2);
    const auto first = tracker.update({Detection{firstBox, 0.9F, 0, "fish"}});
    const auto second = tracker.update({Detection{{1, 0, 11, 10}, 0.8F, 0, "fish"}});
    requireTest(first.front().id == second.front().id, "track identity preserved");
    requireTest(second.front().confirmed, "track confirmation");
    return 0;
}
