#pragma once

#include "common/tracker_base.hpp"

#include "ORB.hpp"

class OBDTracker : public ORBTracker {
public:
    using ORBTracker::ORBTracker;

    void processFrame(uchar *imageGrayData, size_t cols, size_t rows, bool isGrey) override;
};