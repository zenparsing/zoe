import * as directory from 'directory.js';
import * as timer from 'timer.js';
import * as process from 'process.js';

export async function main(zoe) {
  if (!zoe.sys) {
    throw new Error('zoe.sys is not defined (use --zoe-test-sys-api flag to enable)');
  }
  await directory.test(zoe.sys);
  await timer.test(zoe.sys);
  await process.test(zoe.sys);
}
