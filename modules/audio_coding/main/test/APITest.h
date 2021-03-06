/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_H
#define API_TEST_H

#include "ACMTest.h"
#include "Channel.h"
#include "PCMFile.h"
#include "event_wrapper.h"
#include "utility.h"

enum APITESTAction {TEST_CHANGE_CODEC_ONLY = 0, DTX_TEST = 1};

class APITest : public ACMTest
{
public:
    APITest();
    ~APITest();

    void Perform();
private:
    WebRtc_Word16 SetUp();
    
    static bool PushAudioThreadA(void* obj);
    static bool PullAudioThreadA(void* obj);
    static bool ProcessThreadA(void* obj);
    static bool APIThreadA(void* obj);

    static bool PushAudioThreadB(void* obj);
    static bool PullAudioThreadB(void* obj);
    static bool ProcessThreadB(void* obj);
    static bool APIThreadB(void* obj);

    void CheckVADStatus(char side);

    // Set Min delay, get delay, playout timestamp
    void TestDelay(char side);

    // Unregister a codec & register again.
    void TestRegisteration(char side);

    // Playout Mode, background noise mode.
    // Receiver Frequency, playout frequency.
    void TestPlayout(char receiveSide);

    // set/get receiver VAD status & mode.
    void TestReceiverVAD(char side);

    // 
    void TestSendVAD(char side);

    void CurrentCodec(char side);
    
    void ChangeCodec(char side);
    
    void Wait(WebRtc_UWord32 waitLengthMs);

    void LookForDTMF(char side);

    void RunTest(char thread);
    
    bool PushAudioRunA();    
    bool PullAudioRunA();
    bool ProcessRunA();
    bool APIRunA();
  
    bool PullAudioRunB();
    bool PushAudioRunB();
    bool ProcessRunB();
    bool APIRunB();



    //--- ACMs
    AudioCodingModule* _acmA;
    AudioCodingModule* _acmB;
    
    //--- Channels
    Channel* _channel_A2B;
    Channel* _channel_B2A;
    
    //--- I/O files
    // A
    PCMFile _inFileA;
    PCMFile _outFileA;
    // B
    PCMFile _outFileB;
    PCMFile _inFileB;
    
    //--- I/O params
    // A
    WebRtc_Word32 _outFreqHzA;
    // B
    WebRtc_Word32 _outFreqHzB;
    
    // Should we write to file.
    // we might skip writing to file if we
    // run the test for a long time.
    bool _writeToFile;
    //--- Events
    // A
    EventWrapper* _pullEventA;      // pulling data from ACM
    EventWrapper* _pushEventA;      // pushing data to ACM
    EventWrapper* _processEventA;   // process
    EventWrapper* _apiEventA;       // API calls
    // B
    EventWrapper* _pullEventB;      // pulling data from ACM
    EventWrapper* _pushEventB;      // pushing data to ACM
    EventWrapper* _processEventB;   // process
    EventWrapper* _apiEventB;       // API calls

    // keep track of the codec in either side.
    WebRtc_UWord8 _codecCntrA;
    WebRtc_UWord8 _codecCntrB;

    // keep track of tests
    WebRtc_UWord8 _testCntrA;
    WebRtc_UWord8 _testCntrB;

    // Is set to true if there is no encoder in either side
    bool _thereIsEncoderA;
    bool _thereIsEncoderB;
    bool _thereIsDecoderA;
    bool _thereIsDecoderB;

    bool             _sendVADA;
    bool             _sendDTXA;
    ACMVADMode       _sendVADModeA;

    bool             _sendVADB;
    bool             _sendDTXB;
    ACMVADMode       _sendVADModeB;

    WebRtc_Word32    _minDelayA;
    WebRtc_Word32    _minDelayB;
    bool             _payloadUsed[32];
        
    AudioPlayoutMode    _playoutModeA;
    AudioPlayoutMode    _playoutModeB;

    ACMBackgroundNoiseMode _bgnModeA;
    ACMBackgroundNoiseMode _bgnModeB;


    WebRtc_UWord64   _receiveVADActivityA[3];
    WebRtc_UWord64   _receiveVADActivityB[3];
    bool           _verbose;
    
    int            _dotPositionA;
    int            _dotMoveDirectionA;
    int            _dotPositionB;
    int            _dotMoveDirectionB;

    char           _movingDot[41];
    
    DTMFDetector*  _dtmfCallback;
    VADCallback*   _vadCallbackA;
    VADCallback*   _vadCallbackB;
    RWLockWrapper&    _apiTestRWLock;
    bool           _randomTest;
    int            _testNumA;
    int            _testNumB;
};


#endif
