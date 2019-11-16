// @MAIN_JS_HEADER@

(sys) => {

  function print(...args) {
    for (let i = 0; i < args.length; ++i) {
      if (i > 0) sys.stdout(' ');
      sys.stdout(args[i]);
    }
    sys.stdout('\n');
  }

  let hostAPI = {
    cwd() { return sys.cwd(); },
    args() { return Array.from(sys.args); },
  };

  function loadModule(url, callback) {
    try {
      callback(null, sys.readTextFileSync(url));
    } catch (err) {
      callback(new Error(`Unable to load module (${ url })`));
    }
  }

  function main() {
    if (sys.args.length < 2) {
      print('zoe - A modern JavaScript runtime');
      print('');
      print('usage: zoe filename');
      return;
    }

    const url = sys.resolveFilePath(sys.args[1], sys.cwd());
    import(url).then(ns => {
      if (typeof ns.main === 'function') {
        ns.main(hostAPI);
      }
    });
  }

  sys.global.print = print;

  return { main, loadModule };

};

// @MAIN_JS_FOOTER@
