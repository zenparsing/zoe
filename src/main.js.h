// AUTOGENERATED
static const std::string main_js = u8R"js(

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
      callback(new Error(`Unable to load module (${ url }) - ${ err.message }`));
    }
  }

  function main() {
    if (sys.args.length > 1) {
      const path = sys.args[1];
      // TODO: Handle invalid URLs?
      const url = sys.resolveFilePath(sys.args[1], sys.cwd());
      return import(url).then(ns => {
        if (typeof ns.main === 'function') {
          ns.main(hostAPI);
        }
      });
    }
  }

  sys.global.print = print;

  return { main, loadModule };

};

// )js";
