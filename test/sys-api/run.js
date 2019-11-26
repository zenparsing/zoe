import * as directory from 'directory.js';
import * as timer from 'timer.js';
import * as process from 'process.js';

export async function main(zoe) {
  await directory.test(zoe.sys);
  await timer.test(zoe.sys);
  await process.test(zoe.sys);
}
