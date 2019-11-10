// @MAIN_JS_HEADER@

(sys) => {

  Promise.resolve(1).then(x => {
    sys.stdout("hello from promise!\n");
  });

  function print(...args) {
    for (let i = 0; i < args.length; ++i) {
      if (i > 0) sys.stdout(' ');
      sys.stdout(args[i]);
    }
    sys.stdout('\n');
  }

  sys.global.print = print;

  import('https://server/path/to/foo.js');

};

// @MAIN_JS_FOOTER@
