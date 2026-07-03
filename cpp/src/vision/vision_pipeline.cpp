#include "vision/vision_pipeline.h"

#include <utility>

namespace gcs::vision {

using interfaces::TrackResult;
using interfaces::VideoFrame;

VisionPipeline::VisionPipeline(std::unique_ptr<interfaces::IObjectDetector> detector,
                               std::unique_ptr<interfaces::ITracker> tracker,
                               std::unique_ptr<interfaces::ITrackingPolicy> policy)
    : m_detector(std::move(detector)),
      m_tracker(std::move(tracker)),
      m_policy(std::move(policy))
{
}

void VisionPipeline::setTrackingEnabled(bool on)
{
    m_tracking = on;
    if (!on)
        m_tracker->reset();
}

TrackResult VisionPipeline::process(const VideoFrame &frame, interfaces::ICommandSink *sink)
{
    TrackResult res;
    res.timestampMs = frame.timestampMs;
    res.detections = m_detector->detect(frame);
    res.target = m_tracker->update(frame, res.detections);

    if (m_tracking && res.target) {
        if (sink)
            m_policy->steer(*res.target, *sink);
    } else if (!res.target) {
        m_policy->onLost();
    }
    return res;
}

} // namespace gcs::vision
