# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../../../common_settings.gypi', # Common settings
  ],
  'targets': [
    {
      'target_name': 'webrtc_vp8',
      'type': '<(library)',
      'dependencies': [
        '../../../../../../system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '../interface',
        '../../../../../../common_video/interface',
        '../../../interface',
        '../../../../../interface',
      ],
      'conditions': [
        ['build_with_chromium==1', {
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                # We don't want to link with the static library inside Chromium
                # on Windows. Chromium uses the ffmpeg DLL and exports the
                # necessary libvpx symbols for us.
                '../../../../../../../libvpx/libvpx.gyp:libvpx_include',
              ],
            },{
              'dependencies': [
                '../../../../../../../libvpx/libvpx.gyp:libvpx',
              ],
              'include_dirs': [
                '../../../../../../../libvpx/source/libvpx',
              ],
            }],
          ],
        },{
          'dependencies': [
            '../../../../../../../third_party/libvpx/libvpx.gyp:libvpx',
          ],
          'include_dirs': [
            '../../../../../../../third_party/libvpx',
          ],
        }],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
          '../../../../../../common_video/interface',
          '../../../interface',
        ],
      },
      'sources': [
        '../interface/vp8.h',
        'vp8.cc',
      ],
    },

    {
      'target_name': 'vp8_test',
      'type': 'executable',
      'dependencies': [
        'webrtc_vp8',
        '../../../../../../system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '../../../test_framework/test_framework.gyp:test_framework',
        '../../../../../../common_video/vplib/main/source/vplib.gyp:webrtc_vplib'
      ],
      'sources': [
        # header files
        '../test/benchmark.h',
        '../test/normal_async_test.h',
        '../test/packet_loss_test.h',
        '../test/unit_test.h',
        '../test/dual_decoder_test.h',

        # source files
        '../test/benchmark.cc',
        '../test/normal_async_test.cc',
        '../test/packet_loss_test.cc',
        '../test/tester.cc',
        '../test/unit_test.cc',
        '../test/dual_decoder_test.cc',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
