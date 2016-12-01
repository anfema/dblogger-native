const { Logger } = require('bindings')('dblogger')
module.exports = (options) => new Logger(options);
