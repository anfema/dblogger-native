var { Logger } = require('bindings')('dblogger');

let logger = new Logger({
	type: "sqlite",
	name: "./test.db",
	stdout: true,
});
console.log(logger);

process.nextTick(() => {

	logger.tag('tag1', 'tag2').tag('subTag1').tag('bla').tag('tag1');

	let logger2 = new Logger({ stdout: true })
	let logger3 = new Logger(30);

	logger.info("Test", 2, { something: "something other" }, [0, 1, 2]);

	function bla() {
		logger2.debug('From function');
	}

	bla();

});

setTimeout(() => {
	console.log('exiting...')
}, 3000);

//let obj2 = dblogger.Logger('tag')
