#ifndef COMMON_SATELLITEVIEW_H
#define COMMON_SATELLITEVIEW_H

#include <cstddef>
#include <memory>

class SatelliteView {
public:
	virtual ~SatelliteView() {}
    virtual char getObjectAt(size_t x, size_t y) const = 0;
    virtual std::unique_ptr<SatelliteView> clone() const = 0;
};

#endif // COMMON_SATELLITEVIEW_H
