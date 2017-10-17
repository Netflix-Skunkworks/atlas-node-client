{
  'targets': [
  {
    'target_name': 'atlas',
      'dependencies': ['libatlas'],
      'sources': ["<!@(ls -1 src/*.cc)"],
      'libraries': [ 
         "-L<(module_root_dir)/build/Release/",
        "-Wl,-rpath,\$$ORIGIN",
        "-latlasclient"
      ],
      'include_dirs' : [
        "<!(node -e \"require('nan')\")",
        'nc/root/include'
        ],
      'conditions': [
        [ 'OS=="mac"', {
          'xcode_settings': {
            'OTHER_CPLUSPLUSFLAGS' : ['-stdlib=libc++', '-v', '-std=c++11', '-Wall', '-Wextra', '-Wno-unused-parameter', '-g', '-O2' ],
            'OTHER_LDFLAGS': ['-stdlib=libc++'], 
            'MACOSX_DEPLOYMENT_TARGET': '10.12',
            'GCC_ENABLE_CPP_EXCEPTIONS': 'NO'
          }
        }],
        ['OS=="linux"', {
          'cflags': ['-std=c++11', '-Wall', '-Wextra', '-Wno-unused-parameter', '-g', '-O2' ]
        }]
      ]
  },
  {
    'target_name': 'libatlas',
    'type': 'none',
    'actions': [{
      'action_name': 'install_libatlasclient',
      'inputs': [''],
      'outputs': [''],
      'action': ['node', 'scripts/install-lib.js']
    }]
  },
  ]
}

