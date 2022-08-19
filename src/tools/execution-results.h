/*
 * Copyright 2017 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// Shared execution result checking code
//

#include "shell-interface.h"
#include "wasm.h"

namespace wasm {

typedef std::vector<Literal> Loggings;

// Logs every relevant import call parameter.
struct LoggingExternalInterface : public ShellExternalInterface {
  Loggings& loggings;

  struct State {
    // Legalization for JS emits get/setTempRet0 calls ("temp ret 0" means a
    // temporary return value of 32 bits; "0" is the only important value for
    // 64-bit legalization, which needs one such 32-bit chunk in addition to
    // the normal return value which can handle 32 bits).
    uint32_t tempRet0 = 0;
  } state;

  LoggingExternalInterface(Loggings& loggings) : loggings(loggings) {}

  Literals callImport(Function* import, Literals& arguments) override {
    if (import->module == "fuzzing-support") {
      std::cout << "[LoggingExternalInterface logging";
      loggings.push_back(Literal()); // buffer with a None between calls
      for (auto argument : arguments) {
        if (argument.type == Type::i64) {
          // To avoid JS legalization changing logging results, treat a logging
          // of an i64 as two i32s (which is what legalization would turn us
          // into).
          auto low = Literal(int32_t(argument.getInteger()));
          auto high = Literal(int32_t(argument.getInteger() >> int32_t(32)));
          std::cout << ' ' << low;
          loggings.push_back(low);
          std::cout << ' ' << high;
          loggings.push_back(high);
        } else {
          std::cout << ' ' << argument;
          loggings.push_back(argument);
        }
      }
      std::cout << "]\n";
      return {};
    } else if (import->module == ENV) {
      if (import->base == "log_execution") {
        std::cout << "[LoggingExternalInterface log-execution";
        for (auto argument : arguments) {
          std::cout << ' ' << argument;
        }
        std::cout << "]\n";
        return {};
      } else if (import->base == "setTempRet0") {
        state.tempRet0 = arguments[0].geti32();
        return {};
      } else if (import->base == "getTempRet0") {
        return {Literal(state.tempRet0)};
      }
    }
    std::cerr << "[LoggingExternalInterface ignoring an unknown import "
              << import->module << " . " << import->base << '\n';
    return {};
  }
};

// gets execution results from a wasm module. this is useful for fuzzing
//
// we can only get results when there are no imports. we then call each method
// that has a result, with some values
struct ExecutionResults {
  struct Trap {};
  struct Exception {};
  using FunctionResult = std::variant<Literals, Trap, Exception>;

  struct ExportExecutionInfo {
    // The export name that was executed.
    Name name;

    // The result of running the function, which is either a Literals, or a trap
    // or an exception.
    FunctionResult result;

    // Logged values using the logging APIs. These are side effects that must
    // compare equal.
    Loggings loggings;
  };

  std::vector<ExportExecutionInfo> infos;

  // Loggings that happen during global execution, such as the start function,
  // which are not tied to calling an export
  Loggings globalLoggings;

  // If set, we should ignore this and not compare it to anything.
  bool ignore = false;
  // If set, we are in a mode where traps can be ignored by the optimizer. That
  // means that if trap doesn't happen, we can compare normally, but if a trap
  // does then we should stop comparing from there, since the optimizer is free
  // to handle code that traps like undefined behavior, and might cause side
  // effects that are noticeable later.
  bool optimizerIgnoresTraps = false;

  ExecutionResults(const PassOptions& options)
    : optimizerIgnoresTraps(options.ignoreImplicitTraps ||
                            options.trapsNeverHappen) {}
  ExecutionResults(bool optimizerIgnoresTraps)
    : optimizerIgnoresTraps(optimizerIgnoresTraps) {}

  // get results of execution
  void get(Module& wasm) {
    LoggingExternalInterface interface(globalLoggings);
    try {
      ModuleRunner instance(wasm, &interface);
      // execute all exported methods (that are therefore preserved through
      // opts)
      for (auto& exp : wasm.exports) {
        if (exp->kind != ExternalKind::Function) {
          continue;
        }
        std::cout << "[fuzz-exec] calling " << exp->name << "\n";
        auto* func = wasm.getFunction(exp->value);
        ExportExecutionInfo info;
        info.name = exp->name;
        run(func, wasm, instance, info);
        if (auto* values = std::get_if<Literals>(&info.result)) {
          // ignore the result if we hit an unreachable and returned no value
          if (values->size() > 0) {
            std::cout << "[fuzz-exec] note result: " << exp->name << " => ";
            auto resultType = func->getResults();
            if (resultType.isRef()) {
              // Don't print reference values, as funcref(N) contains an index
              // for example, which is not guaranteed to remain identical after
              // optimizations.
              std::cout << resultType << '\n';
            } else {
              std::cout << *values << '\n';
            }
          }
        }
      }
    } catch (const TrapException&) {
      // may throw in instance creation (init of offsets)
    }
  }

  // get current results and check them against previous ones
  void check(Module& wasm) {
    ExecutionResults optimizedResults(optimizerIgnoresTraps);
    optimizedResults.get(wasm);
    if (optimizedResults != *this) {
      std::cout << "[fuzz-exec] optimization passes changed results\n";
      exit(1);
    }
  }

  bool areEqual(Literal a, Literal b) {
    // We allow nulls to have different types (as they compare equal regardless)
    // but anything else must have an identical type.
    // We cannot do this in nominal typing, however, as different modules will
    // have different types in general. We could perhaps compare the entire
    // graph structurally TODO
    if (getTypeSystem() != TypeSystem::Nominal) {
      if (a.type != b.type && !(a.isNull() && b.isNull())) {
        std::cout << "types not identical! " << a << " != " << b << '\n';
        return false;
      }
    }
    if (a.type.isRef()) {
      // Don't compare references - only their types. There are several issues
      // here that we can't fully handle, see
      // https://github.com/WebAssembly/binaryen/issues/3378, but the core issue
      // is that we are comparing results between two separate wasm modules (and
      // a separate instance of each) - we can't really identify an identical
      // reference between such things. We can only compare things structurally,
      // for which we compare the types.
      return true;
    }
    if (a != b) {
      std::cout << "values not identical! " << a << " != " << b << '\n';
      return false;
    }
    return true;
  }

  bool areEqual(Literals a, Literals b) {
    if (a.size() != b.size()) {
      std::cout << "literal counts not identical! " << a << " != " << b << '\n';
      return false;
    }
    for (Index i = 0; i < a.size(); i++) {
      if (!areEqual(a[i], b[i])) {
        return false;
      }
    }
    return true;
  }

  bool compareLoggings(const Loggings& a, const Loggings& b) {
    if (a.size() != b.size()) {
      std::cout << "logging counts not identical!\n";
      return false;
    }
    for (Index i = 0; i < a.size(); i++) {
      if (!areEqual(a[i], b[i])) {
        return false;
      }
    }
    return true;
  }

  bool operator==(ExecutionResults& other) {
    if (ignore || other.ignore) {
      std::cout << "ignoring comparison of ExecutionResults!\n";
      return true;
    }
    if (!compareLoggings(globalLoggings, other.globalLoggings)) {
      std::cout << "[fuzz-exec] global loggings did not match\n";
      return false;
    }

    if (infos.size() != other.infos.size()) {
      std::cout << "[fuzz-exec] number of export executions did not match\n";
      return false;
    }
    for (Index i = 0; i < infos.size(); i++) {
      auto& info = infos[i];
      auto& otherInfo = other.infos[i];
      std::cout << "[fuzz-exec] comparing " << info.name << " ("
                << otherInfo.name << ")\n";
      if (info.name != otherInfo.name) {
        std::cout << "[fuzz-exec] export name did not match\n";
        return false;
      }
      // We should see see same result kind: both Literals, or both Traps, or
      // both Exceptions.
      if (info.result.index() != otherInfo.result.index()) {
        // If we are running in a mode where the optimizer can ignore traps, and
        // one is a trap then we can't report an error
        // here since the optimizer considered a trap to be undefined behavior.
        // We must also stop comparing here since anything later might be
        // affected by side effects of that undefined behavior.
        if (optimizerIgnoresTraps && (std::get_if<Trap>(&info.result) ||
                                      std::get_if<Trap>(&otherInfo.result))) {
          std::cout << "[fuzz-exec] comparing results in a trap-ignoring mode "
                    << "and a trap happened, so ignoring results from here\n";
          return true;
        }

        std::cout << "[fuzz-exec] Function result kind did not match\n";
        return false;
      }
      auto* values = std::get_if<Literals>(&info.result);
      auto* otherValues = std::get_if<Literals>(&otherInfo.result);
      if (values && otherValues && !areEqual(*values, *otherValues)) {
        return false;
      }
      if (!compareLoggings(info.loggings, otherInfo.loggings)) {
        std::cout << "[fuzz-exec] loggings did not match\n";
        return false;
      }
    }
    return true;
  }

  bool operator!=(ExecutionResults& other) { return !((*this) == other); }

  ExportExecutionInfo run(Name exportName, Function* func, Module& wasm) {
    ExportExecutionInfo info;
    info.name = exportName;
    LoggingExternalInterface interface(info.loggings);
    try {
      ModuleRunner instance(wasm, &interface);
      info.result = run(func, wasm, instance, info);
    } catch (const TrapException&) {
      // may throw in instance creation (init of offsets)
    }
    return info;
  }

  FunctionResult run(Function* func, Module& wasm, ModuleRunner& instance, ExportExecutionInfo& info) {
    // The call will append to globalLoggings, so we must note how many exist
    // before us: anything new will be ours.
    struct LoggingMonitor {
      ExportExecutionInfo& info;
      Loggings& globalLoggings;
      size_t initialNum;

      LoggingMonitor(ExportExecutionInfo& info, Loggings& globalLoggings) : info(info), globalLoggings(globalLoggings), initialNum(globalLoggings.size()) {}
      ~LoggingMonitor() {
        auto finalNum = globalLoggings.size();
        for (size_t i = initialNum; i < finalNum; i++) {
          info.loggings.push_back(globalLoggings[i]);
        }
        globalLoggings.resize(initialNum);
      }
    } monitor(info, globalLoggings);

    try {
      Literals arguments;
      // init hang support, if present
      if (auto* ex = wasm.getExportOrNull("hangLimitInitializer")) {
        instance.callFunction(ex->value, arguments);
      }
      // call the method
      for (const auto& param : func->getParams()) {
        // zeros in arguments TODO: more?
        if (!param.isDefaultable()) {
          std::cout << "[trap fuzzer can only send defaultable parameters to "
                       "exports]\n";
          return Trap{};
        }
        arguments.push_back(Literal::makeZero(param));
      }
      return instance.callFunction(func->name, arguments);
    } catch (const TrapException&) {
      return Trap{};
    } catch (const WasmException& e) {
      std::cout << "[exception thrown: " << e << "]" << std::endl;
      return Exception{};
    } catch (const HostLimitException&) {
      // This should be ignored and not compared with, as optimizations can
      // change whether a host limit is reached.
      ignore = true;
      return {};
    }
  }
};

} // namespace wasm
