# Expected behaviour

What happens? Does it crash? Are there any error messages on the console?


# Steps to reproduce

For example:

1. Clone this repo: ...
2. Install node packages: `yarn install`
3. Run the app: `node index.js`

Alternatively show us a code sample:

```js
const logger = require('dblogger')({
	type: "sqlite",
	name: "./test.db",
	stdout: true,
});

setInterval(() => {
	logger.log('Ping');
}, 1000);
```

# Environment used

For example:

- operating system: `Ubuntu 17.10`
- node: `9.7.1`
- dblogger: `0.6.0`
- sqlite: `3.19.3`
- postgresql: `9.6.8`
- c++-compiler: gcc `7.2.0`
