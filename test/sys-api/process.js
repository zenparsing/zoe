import { assert } from 'util.js';

export async function test(sys) {
  let process;
  let exitPromise = new Promise((resolve, reject) => {
    // Run zoe without any args
    let cmd = sys.args[0];
    process = sys.spawnProcess(cmd, [cmd], (err, result) => {
      if (err) reject(err);
      else resolve(result);
    });
  });
  assert(typeof process === 'number' && process | 0 > 0, 'returns a process ID');
  let result = await exitPromise;
  assert(await exitPromise === undefined, 'exit callback is called');
}
