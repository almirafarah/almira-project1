#ifndef SATELLITE_VIEW_H
#define SATELLITE_VIEW_H

#include <cstddef>

class SatelliteView {
public:
    virtual ~SatelliteView() {}
    virtual char getObjectAt(size_t x, size_t y) const = 0;
};

#endif // SATELLITE_VIEW_H
