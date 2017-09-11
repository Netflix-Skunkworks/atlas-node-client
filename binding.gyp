{
  'targets': [
  {
    'target_name': 'atlas',
      'dependencies': ['libatlas'],
      'sources': ["<!@(ls -1 src/*.cc)"],
      'cflags': [ '-std=c++11', '-Wall', '-g', '-O2' ],
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
            'OTHER_CPLUSPLUSFLAGS' : ['-std=c++11','-stdlib=libc++', '-v'],
            'OTHER_LDFLAGS': ['-stdlib=libc++'], 
            'MACOSX_DEPLOYMENT_TARGET': '10.12',
            'GCC_ENABLE_CPP_EXCEPTIONS': 'NO'
          }
        }]]
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
