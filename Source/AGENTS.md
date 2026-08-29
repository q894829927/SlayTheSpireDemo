# Source Tree Rules

Applies to `Source/**`.

## Module Boundaries

Runtime production code belongs to `Source/SlayTheSpireDemo/`. Automation-only code belongs to the Editor-only `Source/SlayTheSpireDemoTests/` module.

Production code must never depend on the test module. Keep Automation-only reflected classes out of the runtime module and preserve test-module exclusion from Shipping.

## Unreal C++ Conventions

Use normal Unreal prefixes. Prefer forward declarations and small public headers. UObject runtime ownership must be GC-safe through explicit Outer ownership and appropriate `UPROPERTY`/`TObjectPtr` references. Do not enable Tick by default.

Keep includes and module dependencies explicit and minimal. Do not add plugins, third-party dependencies or change the engine association/build settings without user approval.

## Change Discipline

Inspect relevant files first, make the smallest coherent change and avoid unrelated refactors. Preserve public APIs unless a change is required by the requested contract.

After C++ changes, verify includes/module dependencies and run the smallest relevant build/test set available. Report unavailable validation or failures accurately.
