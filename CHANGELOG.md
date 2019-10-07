### 0.7.1

- Bugfix: Crash when using logger on toplevel without function

### 0.7.0

- Compiles with Node 12, if you get problems with older node versions fall back to `0.6.1`

### 0.6.1

- Fixed typescript definition

### 0.6.0

- Allow instanciation of logger without any DB config (use `type: 'none'`)
- Bugfix: Do not crash with segmentation fault if first logger to be instanciated is provided with no configuration (throws exception now)

### 0.5.6

- Silence the `NOTICE` warnings of `libpq` when skipping table creation on startup

### 0.5.4

- Add `type` property to options in typescript definition

### 0.5.3

- Add TypeScript definition file 🎉

### 0.5.2

- Bugfix: If loglevel was not explicitly set the initialization of the global log level was undefined
- Bugfix: On logfile rotation the `log_to_stdout` flag was lost on reinitialization
- Postgres: Mark connection as invalid on fatal errors, try to reconnect on next log statement
- Postgres: Set application name to `<logger name> logger`

### 0.5.1

- Bugfix: `stdout` setting will now be inherited, you can turn it off for specific loggers by only setting that option on creation

### 0.5.0

- Bugfix: Keep configured log level and re-apply that setting to all loggers that do not override it specifically

### 0.4.6

- Bugfix: Log-rotation/DB-reconnect killed configured logger name

### 0.4.0

-  Implement PostgreSQL support, speed up logging by not failing a lot of insert statements

### 0.3.1

- SQLite: Turn of FSYNC and autovacuum to speed up processing

### 0.3.0

- Support log rotation

### 0.2.4

- Bugfix: Tags were applied to wrong loggers

### 0.2.3

- Make it work on Linux in addition to MacOS

### 0.2.0

- Initial public release
