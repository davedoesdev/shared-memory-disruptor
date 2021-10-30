{
  "targets": [
    {
      "target_name": "disruptor",
      "sources": [ "src/disruptor.cc" ],
      "include_dirs": ["<!@(node -p \"require('node-addon-api').include\")"],
      "dependencies": ["<!(node -p \"require('node-addon-api').gyp\")"],

      'cflags+': [ '-std=gnu++14', '-Wall', '-Wextra', '-Werror' ],
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions', '-std=gnu++0x' ],
      'xcode_settings': {
        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
        'CLANG_CXX_LIBRARY': 'libc++',
        'MACOSX_DEPLOYMENT_TARGET': '10.7',
        'CLANG_CXX_LANGUAGE_STANDARD': 'c++17',
        'WARNING_CFLAGS': [ '-Wall, -Wextra' ],
        'GCC_TREAT_WARNINGS_AS_ERRORS': 'YES'
      },
      'msvs_settings': {
        'VCCLCompilerTool': { 'ExceptionHandling': 1 },
      },
      'conditions': [
        [
          'OS == "linux"',
          {
            'libraries': [ '-lrt' ],
          }
        ],
        [
          'coverage == "true"',
          {
            'cflags+': [ '--coverage', '-DCOVERAGE=1' ],
            'ldflags+': [ '--coverage' ]
          }
        ]
      ]
    }
  ]
}
