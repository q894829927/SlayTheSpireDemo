# Source Tree Rules

Applies to `Source/**`.

## Module Boundaries

Runtime production code belongs to `Source/SlayTheSpireDemo/`. Automation-only code belongs to the Editor-only `Source/SlayTheSpireDemoTests/` module.

Production code must never depend on the test module. Keep Automation-only reflected classes out of the runtime module and preserve test-module exclusion from Shipping.

## Unreal C++ Conventions

Use normal Unreal prefixes. Prefer forward declarations and small public headers. UObject runtime ownership must be GC-safe through explicit Outer ownership and appropriate `UPROPERTY`/`TObjectPtr` references. Do not enable Tick by default.

Keep includes and module dependencies explicit and minimal. Plugin, engine-association and build-setting changes require user authorization. Root scope rules and `docs/ValidationExecutionPolicy.md` apply.
