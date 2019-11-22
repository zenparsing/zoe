import { assert } from 'util.js';

function wait(sys, ms) {
  return new Promise(resolve => {
    let start = Date.now();
    sys.startTimer(ms, 0, () => {
      resolve(Date.now() - start);
    });
  });
}

export async function test(sys) {
  let ms = await wait(sys, 5);
  assert(ms >= 4, 'timeout works');

  let counter;
  let timer;

  counter = 0;
  timer = sys.startTimer(1, 0, () => counter += 1);
  sys.stopTimer(timer);
  await wait(sys, 5);
  assert(counter === 0, 'stop timer');

  counter = 0;
  timer = sys.startTimer(0, 1, () => counter += 1);
  sys.startTimer(5, 0, () => sys.stopTimer(timer));
  await wait(sys, 10);
  assert(counter > 1 && counter < 5, 'stop repeating timer');
}
