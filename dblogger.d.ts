// template: http://www.typescriptlang.org/docs/handbook/declaration-files/templates/module-function-d-ts.html

declare namespace dblogger {
	enum Dummy {
		// This is necessary so TS emits a require statement, since all other types in this
		// namespace are compiled away.
	}

	export const enum LogLevel {
		Trace = 10,
		Debug = 20,
		Info = 30,
		Log = 30,
		Warn = 40,
		Error = 50,
		Fatal = 60
	}

	export interface BaseOptions {
		type: 'postgres' | 'sqlite' | 'none',
		level: LogLevel,
		/** Also log to stdout */
		stdout: boolean,
		/** Logger name */
		logger: string,
	}

	export interface NoneOptions extends BaseOptions {
		type: 'none',
	}

	export interface PostgresOptions extends BaseOptions {
		type: 'postgres',
		/** Database name */
		name: string,
		user: string,
		password: string,
		host: string,
		port: number,
	}

	export interface SqliteOptions extends BaseOptions {
		type: 'sqlite',
		/** File name for sqlite */
		name: string,
	}

	export type Options = PostgresOptions | SqliteOptions | NoneOptions;

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
