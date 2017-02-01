{
  "targets": [
    {
      "target_name": "dblogger",
      "sources": [
        "cpp/main.cc",
        "cpp/logger.cc",
        "cpp/db.cc",
        "cpp/db_logger.cc",
        "cpp/stdout_logger.cc"
      ],
      'variables': {
        'pgconfig': 'pg_config'
      },
      "include_dirs": [
        '<!@(<(pgconfig) --includedir)',
        '/usr/include/'
      ],
      "conditions": [
        [
          'OS=="win"', {
            'libraries' : ['libpq.lib', 'libsqlite3.lib'],
            'msvs_settings': {
              'VCLinkerTool' : {
                'AdditionalLibraryDirectories' : [
                  '<!@(<(pgconfig) --libdir)\\'
                ]
              },
            }
          },
          'OS=="mac"', {
            'libraries' : ['-lpq -L<!@(<(pgconfig) --libdir) -lsqlite3 -L/usr/lib'],
            "xcode_settings": {
                'OTHER_CPLUSPLUSFLAGS' : ['-std=c++11','-stdlib=libc++'],
                'OTHER_LDFLAGS': ['-stdlib=libc++'],
                'MACOSX_DEPLOYMENT_TARGET': '10.7' }
            },
          { # Other OS
             'libraries' : ['-lpq -L<!@(<(pgconfig) --libdir) -lsqlite3 -L/usr/lib']
          }
        ]
      ]
    }
  ]
}
