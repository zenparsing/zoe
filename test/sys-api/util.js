export function asyncify(fn) {
  return function(...args) {
    return new Promise((resolve, reject) => {
      fn(...args, (err, result) => {
        if (err) reject(err);
        else resolve(result);
      });
    });
  };
}

class AssertionError extends Error {
  constructor(message, actual, expected) {
    super(`Assertion failed: ${ message }`);
    this.name = 'AssertionError';
    this.actual = actual;
    this.expected = expected;
  }
}

export function assert(x, message) {
  if (!x) {
    throw new AssertionError(message, Boolean(x), true);
  }
}
