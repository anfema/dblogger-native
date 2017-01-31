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
      "include_dirs": [
        '/usr/include/',
		'/usr/local/include/'
      ],
      "conditions": [
        [
          'OS=="mac"', {
            'libraries' : ['-lsqlite3 -L/usr/lib -lpq'],
            "xcode_settings": {
                'OTHER_CPLUSPLUSFLAGS' : ['-std=c++11','-stdlib=libc++'],
                'OTHER_LDFLAGS': ['-stdlib=libc++'],
                'MACOSX_DEPLOYMENT_TARGET': '10.7' }
            },
          { # Other OS
             'libraries' : ['-lsqlite3 -L/usr/lib -std=c++11']
          }
        ]
      ]
    }
  ]
}
