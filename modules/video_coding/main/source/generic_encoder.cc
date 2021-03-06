/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "encoded_frame.h"
#include "generic_encoder.h"
#include "media_optimization.h"
#include "../../../../engine_configurations.h"

namespace webrtc {

//#define DEBUG_ENCODER_BIT_STREAM

VCMGenericEncoder::VCMGenericEncoder(VideoEncoder& encoder, bool internalSource /*= false*/)
:
_encoder(encoder),
_codecType(kVideoCodecUnknown),
_VCMencodedFrameCallback(NULL),
_bitRate(0),
_frameRate(0),
_internalSource(false)
{
}


VCMGenericEncoder::~VCMGenericEncoder()
{
}

WebRtc_Word32
VCMGenericEncoder::Reset()
{
    _bitRate = 0;
    _frameRate = 0;
    _VCMencodedFrameCallback = NULL;
    return _encoder.Reset();
}

WebRtc_Word32 VCMGenericEncoder::Release()
{
    _bitRate = 0;
    _frameRate = 0;
    _VCMencodedFrameCallback = NULL;
    return _encoder.Release();
}

WebRtc_Word32
VCMGenericEncoder::InitEncode(const VideoCodec* settings, WebRtc_Word32 numberOfCores, WebRtc_UWord32 maxPayloadSize)
{
    _bitRate = settings->startBitrate;
    _frameRate = settings->maxFramerate;
    _codecType = settings->codecType;
    if (_VCMencodedFrameCallback != NULL)
    {
        _VCMencodedFrameCallback->SetCodecType(_codecType);
    }
    return _encoder.InitEncode(settings, numberOfCores, maxPayloadSize);
}

WebRtc_Word32
VCMGenericEncoder::Encode(const VideoFrame& inputFrame,
                          const CodecSpecificInfo* codecSpecificInfo,
                          FrameType frameType)
{
    RawImage rawImage(inputFrame.Buffer(), inputFrame.Length(), inputFrame.Size());
    rawImage._width     = inputFrame.Width();
    rawImage._height    = inputFrame.Height();
    rawImage._timeStamp = inputFrame.TimeStamp();

    WebRtc_Word32 ret = _encoder.Encode(rawImage, codecSpecificInfo, VCMEncodedFrame::ConvertFrameType(frameType));

    return ret;
}

WebRtc_Word32
VCMGenericEncoder::SetPacketLoss(WebRtc_Word32 packetLoss)
{
    return _encoder.SetPacketLoss(packetLoss);
}

WebRtc_Word32
VCMGenericEncoder::SetRates(WebRtc_UWord32 newBitRate, WebRtc_UWord32 frameRate)
{
    WebRtc_Word32 ret = _encoder.SetRates(newBitRate, frameRate);
    if (ret < 0)
    {
        return ret;
    }
    _bitRate = newBitRate;
    _frameRate = frameRate;
    return VCM_OK;
}

WebRtc_Word32
VCMGenericEncoder::CodecConfigParameters(WebRtc_UWord8* buffer, WebRtc_Word32 size)
{
    WebRtc_Word32 ret = _encoder.CodecConfigParameters(buffer, size);
    if (ret < 0)
    {
        return ret;
    }
    return ret;
}

WebRtc_UWord32 VCMGenericEncoder::BitRate() const
{
    return _bitRate;
}

WebRtc_UWord32 VCMGenericEncoder::FrameRate() const
{
    return _frameRate;
}

WebRtc_Word32
VCMGenericEncoder::SetPeriodicKeyFrames(bool enable)
{
    return _encoder.SetPeriodicKeyFrames(enable);
}

WebRtc_Word32
VCMGenericEncoder::RequestFrame(FrameType frameType)
{
    RawImage image;
    return _encoder.Encode(image, NULL, VCMEncodedFrame::ConvertFrameType(frameType));
}

WebRtc_Word32
VCMGenericEncoder::RegisterEncodeCallback(VCMEncodedFrameCallback* VCMencodedFrameCallback)
{
   _VCMencodedFrameCallback = VCMencodedFrameCallback;

   _VCMencodedFrameCallback->SetCodecType(_codecType);
   _VCMencodedFrameCallback->SetInternalSource(_internalSource);
   return _encoder.RegisterEncodeCompleteCallback(_VCMencodedFrameCallback);
}

bool
VCMGenericEncoder::InternalSource() const
{
    return _internalSource;
}

 /***************************
  * Callback Implementation
  ***************************/
VCMEncodedFrameCallback::VCMEncodedFrameCallback():
_sendCallback(),
_encodedBytes(0),
_payloadType(0),
_bitStreamAfterEncoder(NULL)
{
#ifdef DEBUG_ENCODER_BIT_STREAM
    _bitStreamAfterEncoder = fopen("encoderBitStream.bit", "wb");
#endif
}

VCMEncodedFrameCallback::~VCMEncodedFrameCallback()
{
#ifdef DEBUG_ENCODER_BIT_STREAM
    fclose(_bitStreamAfterEncoder);
#endif
}

WebRtc_Word32
VCMEncodedFrameCallback::SetTransportCallback(VCMPacketizationCallback* transport)
{
    _sendCallback = transport;
    return VCM_OK;
}

WebRtc_Word32
VCMEncodedFrameCallback::Encoded(
    EncodedImage &encodedImage,
    const CodecSpecificInfo* codecSpecificInfo,
    const RTPFragmentationHeader* fragmentationHeader)
{
    FrameType frameType = VCMEncodedFrame::ConvertFrameType(encodedImage._frameType);

    WebRtc_UWord32 encodedBytes = 0;
    if (_sendCallback != NULL)
    {
            encodedBytes = encodedImage._length;

        if (_bitStreamAfterEncoder != NULL)
        {
            fwrite(encodedImage._buffer, 1, encodedImage._length, _bitStreamAfterEncoder);
        }

        RTPVideoTypeHeader rtpTypeHeader;
        RTPVideoTypeHeader* rtpTypeHeaderPtr = &rtpTypeHeader;
        if (codecSpecificInfo)
        {
            CopyCodecSpecific(*codecSpecificInfo, &rtpTypeHeaderPtr);
        }
        else
        {
            rtpTypeHeaderPtr = NULL;
        }

        WebRtc_Word32 callbackReturn = _sendCallback->SendData(
            frameType,
            _payloadType,
            encodedImage._timeStamp,
            encodedImage._buffer,
            encodedBytes,
            *fragmentationHeader,
            rtpTypeHeaderPtr);
       if (callbackReturn < 0)
       {
           return callbackReturn;
       }
    }
    else
    {
        return VCM_UNINITIALIZED;
    }
    _encodedBytes = encodedBytes;
    _mediaOpt->UpdateWithEncodedData(_encodedBytes, frameType);
    if (_internalSource)
    {
        return _mediaOpt->DropFrame(); // Signal to encoder to drop next frame
    }

    return VCM_OK;
}

WebRtc_UWord32
VCMEncodedFrameCallback::EncodedBytes()
{
    return _encodedBytes;
}

void
VCMEncodedFrameCallback::SetMediaOpt(VCMMediaOptimization *mediaOpt)
{
    _mediaOpt = mediaOpt;
}

void VCMEncodedFrameCallback::CopyCodecSpecific(const CodecSpecificInfo& info,
                                                RTPVideoTypeHeader** rtp) {
    switch (info.codecType)
    {
        //TODO(hlundin): Implement case for kVideoCodecVP8.
        default: {
            // No codec specific info. Change RTP header pointer to NULL.
            *rtp = NULL;
            return;
        }

    }
}

} // namespace webrtc
