import * as directories from 'directories.js';
import * as timers from 'timers.js';

export async function main(zoe) {
  await directories.test(zoe.sys);
  await timers.test(zoe.sys);
}
