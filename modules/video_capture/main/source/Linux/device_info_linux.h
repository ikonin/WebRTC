/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_LINUX_DEVICE_INFO_LINUX_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_LINUX_DEVICE_INFO_LINUX_H_

#include "../video_capture_impl.h"
#include "../device_info_impl.h"

namespace webrtc
{
namespace videocapturemodule
{
class DeviceInfoLinux: public DeviceInfoImpl
{
public:
    DeviceInfoLinux(const WebRtc_Word32 id);
    virtual ~DeviceInfoLinux();
    virtual WebRtc_UWord32 NumberOfDevices();
    virtual WebRtc_Word32 GetDeviceName(WebRtc_UWord32 deviceNumber,
                                      WebRtc_UWord8* deviceNameUTF8,
                                      WebRtc_UWord32 deviceNameLength,
                                      WebRtc_UWord8* deviceUniqueIdUTF8,
                                      WebRtc_UWord32 deviceUniqueIdUTF8Length,
                                      WebRtc_UWord8* productUniqueIdUTF8=0,
                                      WebRtc_UWord32 productUniqueIdUTF8Length=0);
    /*
    * Fills the membervariable _captureCapabilities with capabilites for the given device name.
    */
    virtual WebRtc_Word32 CreateCapabilityMap (const WebRtc_UWord8* deviceUniqueIdUTF8);
    virtual WebRtc_Word32 DisplayCaptureSettingsDialogBox(const WebRtc_UWord8* /*deviceUniqueIdUTF8*/,
                                            const WebRtc_UWord8* /*dialogTitleUTF8*/,
                                            void* /*parentWindow*/,
                                            WebRtc_UWord32 /*positionX*/,
                                            WebRtc_UWord32 /*positionY*/) { return -1;}
    WebRtc_Word32 FillCapabilityMap(int fd);
    WebRtc_Word32 Init();
private:

    bool IsDeviceNameMatches(const char* name, const char* deviceUniqueIdUTF8);
    bool GetMaxFPS(int fd, VideoCaptureCapability* cap);
};
} // namespace videocapturemodule
} // namespace webrtc
#endif // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_LINUX_DEVICE_INFO_LINUX_H_
