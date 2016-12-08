const { Logger } = require('bindings')('dblogger')
module.exports = (options) => new Logger(options);

process.on('SIGHUP', () => {
	const logger = new Logger();
	logger.rotate();
	logger.tag('rotate').info('Logfile rotated');
});
