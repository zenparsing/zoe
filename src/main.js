// @MAIN_JS_HEADER@

(sys) => {

  function print(...args) {
    for (let i = 0; i < args.length; ++i) {
      if (i > 0) sys.stdout(' ');
      sys.stdout(args[i]);
    }
    sys.stdout('\n');
  }

  function loadModule(url) {
    sys.setModuleSource(url, sys.readTextFileSync(url));
  }

  function main() {
    if (sys.args.length > 1) {
      const path = sys.args[1];
      const url = sys.resolveFilePath(sys.args[1], sys.cwd());
      return import(url).then(ns => {
        if (typeof ns.main === 'function') {
          ns.main();
        }
      });
    }
  }

  sys.global.print = print;

  return { main, loadModule };

};

// @MAIN_JS_FOOTER@
