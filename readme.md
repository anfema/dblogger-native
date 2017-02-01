# dblogger

This implements a simple logging interface that is able to stream the log entries to a DB server or local sqlite files.

## Obtaining the package

Just install `dblogger` with `npm install --save dblogger`

## API

### Initialization

Initialize a named logger:

~~~javascript
const logger = require('dblogger')({
	type: "sqlite",
	name: "./test.db",
	stdout: true,
});
~~~

To get access to an already initialized logger just skip the options object:

~~~javascript
const logger = require('dblogger')();
~~~

Available options in the options object:

- `type`: `sqlite` or `postgres` (ask me if you need more)
- `name`: db name to use (path to db file for `sqlite`)
- `host`: db host (invalid for sqlite)
- `port`: port number for db server (invalid for sqlite) (optional)
- `user`: username for db server (invalid for sqlite) (optional)
- `password`: password for db server (invalid for sqlite) (optional)
- `level`: log level (defaults to 0/trace) (optional)
- `tablePrefix`: prefix for logging tables (defaults to `logger`) (optional)
- `stdout`: Mirror all log entries to stdout and stderr (for level >= 50/error) (optional)
- `logger`: Name of the logger (if more than one service logs to the same db, defaults to `default`) (optional)

The logger is a native C++ addon, so all logging is sync. You can be sure that every log entry is in the DB when the log statement returns!

### Usage

#### Log something

~~~javascript
logger.trace('Message');
logger.debug('Message');
logger.info('Message');
logger.log('Message'); // same log level as info
logger.warn('Message');
logger.error('Message');
logger.fatal('Message');
~~~

If you log objects or arrays a JSON representation is logged

#### Set log level

You may set the log level on initialization or later by creating a new instance:

~~~javascript
const logger = require('dblogger')(30);
~~~

#### Define tags for log entry

All log entries may be tagged for easier filtering and searching:

~~~javascript
logger.tag('mytag', 'anothertag').log('Message');
~~~

All tags that are defined will be added to the returned logger instance. You could even do the following:

~~~javascript
const logger = require('dblogger')().tag('globaltag');

logger.log('Message'); // this message will be tagged with `globaltag`
~~~

#### Log-rotation

If you're logging into an SQLite file you may want to rotate the logfiles from time to time.
To do that just use the following procedure:

1. Rename the logfile that the process currently logs into
2. Send a HUP signal to the node process

The logging library will then close the old logfile and start anew with an empty file.
Please do not use "copy and truncate" rotation as this makes SQLite sad (meaning: you
will probably corrupt the "old" file and the logger will crash at the next log statement)

Command line example:

~~~bash
mv logfile.db logfile.db.1
pkill -F pidfile.pid -HUP
~~~

## DB Schema

`TODO`
