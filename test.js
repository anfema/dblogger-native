var { Logger } = require('bindings')('dblogger');

process.on('SIGHUP', () => {
	const logger = new Logger({ stdout: true });
	logger.rotate();
	logger.tag('rotate').info('Logfile rotated');
});

// const logger = new Logger({
// 	type: "none",
// 	stdout: true,
// });

const logger = new Logger({
	type: "sqlite",
	name: "./test.db",
	stdout: true,
});

// const logger = new Logger({
// 	type: "postgres",
// 	name: "johannes",
// 	host: 'localhost',
// 	stdout: true,
// });
console.log(logger);

process.nextTick(() => {

	logger.tag('tag1', 'tag2').tag('subTag1').tag('bla').tag('tag1');

	let logger2 = new Logger({ stdout: false })
	let logger3 = new Logger(30);

	logger.tag('fafafa').info("Test", 2, { something: "something other" }, [0, 1, 2]);

	function bla() {
		logger2.debug('From function');
	}

	bla();

});

setInterval(() => {
	logger.log('Ping');
}, 1000);
