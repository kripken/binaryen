// Support

function logValue(x) {
    console.log("[LoggingExternalInterface logging " + x + "]");
}

function logRef(ref) {
    // Look for VM bugs by using the reference in an API (note: we cannot do
    // +ref or ref+"" as those trap).
    JSON.stringify(ref);
    // If not null, try to read a property, which might exercise an
    // interesting code path.
    if (ref) {
        ref.foobar;
    }
    // Finally, log normally as with all other loggers.
    logValue(ref);
}

// Main

let protoFactory = new Proxy({}, {
    get(target, prop, receiver) {
        // Always return a fresh, empty object.
        return {};
    }
});

let constructors = {};

let imports = {
    "protos": protoFactory,
    "env": { constructors },
    "fuzzing-support": {
        "log-i32": logValue,
        "log-i64": logValue,
        "log-f32": logValue,
        "log-f64": logValue,
        "log-anyref": logRef,
        "log-funcref": logRef,
        "log-externref": logRef,
    },
};

let compileOptions = { builtins: ["js-prototypes"] };

let buffer = readbuffer(arguments[0]);

let { module, instance } =
    await WebAssembly.instantiate(buffer, imports, compileOptions);

// Run all exports

function callFunc(func) {
    // Send the function a null for each parameter. Null can be converted without
    // error to both a number and a reference.
    var args = [];
    for (var i = 0; i < func.length; i++) {
        args.push(null);
    }
    return func.apply(null, args);
}

for (var e of WebAssembly.Module.exports(module)) {
    var key = e.name;
    var value = instance.exports[key];
    console.log(`export ${key}: ${value}`);
    if (typeof value === "function") {
        try {
            var result = callFunc(value);
            console.log(`=> ${result}`);
        } catch (e) {
            console.log(`=> trap (${e})`);
        }
    }
}

