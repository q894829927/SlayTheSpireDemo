"""Record the live capture exposure contract in the interior PIE world."""
import json
import math
import os
import unreal as u

world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
assert world, 'Start L_Interior_LivingKitchen PIE first'
systems = u.GameplayStatics.get_all_actors_of_class(world, u.InteriorPortalSystem)
assert len(systems) == 1, 'Expected exactly one InteriorPortalSystem'
system = systems[0]
portals = [system.get_editor_property('blue_portal'), system.get_editor_property('orange_portal')]
entries = []
for portal in portals:
    value = float(portal.get_editor_property('portal_capture_pre_exposure'))
    entries.append({
        'portal': portal.get_name(),
        'capture_pre_exposure': value,
        'finite_positive': math.isfinite(value) and value > 0.0,
    })
report = {
    'passed': all(item['finite_positive'] for item in entries),
    'error': None,
    'diagnostic_status': system.get_editor_property('fidelity_diagnostic_status'),
    'portals': entries,
}
path = u.Paths.project_saved_dir() + 'PortalExposureNormalizationRuntime.json'
with open(path + '.tmp', 'w') as stream:
    json.dump(report, stream, indent=2)
os.replace(path + '.tmp', path)
