// http://www.typescriptlang.org/docs/handbook/declaration-files/templates/module-function-d-ts.html

declare namespace dblogger {
	export enum LogLevel {
		Trace = 10,
		Debug = 20,
		Info = 30,
		Log = 30,
		Warn = 40,
		Error = 50,
		Fatal = 60
	}

	export interface Options {
		level: LogLevel,
		/** Also log to stdout */
		stdout: boolean,
		/** Logger name */
		logger: string,
	}

	export interface Logger {
		trace(...args: any[]): void;
		debug(...args: any[]): void;
		info(...args: any[]): void;
		log(...args: any[]): void;
		warn(...args: any[]): void;
		error(...args: any[]): void;
		fatal(...args: any[]): void;

		tag(...tag: string[]): Logger;
		rotate(): void;
	}
}

declare function dblogger(arg?: dblogger.Options | dblogger.LogLevel): dblogger.Logger;

export = dblogger;
