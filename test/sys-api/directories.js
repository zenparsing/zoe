import { asyncify, assert } from 'util.js';

export async function test(sys) {
  let url = sys.resolveURL('.', import.meta.url);
  let dir = await asyncify(sys.openDirectory)(url);
  let entries = await asyncify(sys.readDirectory)(dir, 100);
  await asyncify(sys.closeDirectory)(dir);
  assert(entries.length > 1, 'readDirectory returns filenames');
}
