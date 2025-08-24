#ifndef COMMON_SATELLITEVIEW_H
#define COMMON_SATELLITEVIEW_H

#include <cstddef>
#include <memory>

class SatelliteView {
public:
        virtual ~SatelliteView() {}
    [[nodiscard]] virtual char getObjectAt(size_t xCoord, size_t yCoord) const = 0;
    [[nodiscard]] virtual std::unique_ptr<SatelliteView> clone() const = 0;
};

#endif // COMMON_SATELLITEVIEW_H
