"""Create player slice/light-function variants; preserve the original interior materials.

Run through the UE Python plugin. Re-run after the native presentation component is
compiled to bind the variants to the authored portal system and save that map.
"""
import json
import unreal as u

E = u.MaterialEditingLibrary
ROOT = '/Game/SlayTheSpireDemo/Interior/Portals'
mapping = {}
created = []


def expression(material, cls, **properties):
    node = E.create_material_expression(material, cls)
    for name, value in properties.items():
        node.set_editor_property(name, value)
    return node


def vector(material, name, value):
    return expression(material, u.MaterialExpressionVectorParameter,
                      parameter_name=name, default_value=u.LinearColor(*value))


def custom(material, code, inputs):
    slots = []
    for name in inputs:
        slot = u.CustomInput()
        slot.set_editor_property('input_name', name)
        slots.append(slot)
    node = expression(material, u.MaterialExpressionCustom, code=code,
                      output_type=u.CustomMaterialOutputType.CMOT_FLOAT1, inputs=slots)
    for name, source in inputs.items():
        assert E.connect_material_expressions(source, '', node, name)
    return node


for name in ('Blue', 'Plaster', 'Black', 'Steel', 'Ivory'):
    source_path = '/Game/House/InteriorMaterials/M_Interior_' + name
    target_path = ROOT + '/M_PlayerSlice_' + name
    source = u.load_asset(source_path)
    assert isinstance(source, u.Material), source_path
    if u.EditorAssetLibrary.does_asset_exist(target_path):
        target = u.load_asset(target_path)
    else:
        target = u.EditorAssetLibrary.duplicate_asset(source_path, target_path)
        assert target
        target.set_editor_property('blend_mode', u.BlendMode.BLEND_MASKED)
        world = expression(target, u.MaterialExpressionWorldPosition)
        origin = vector(target, 'SliceOrigin', (0, 0, 0, 0))
        normal = vector(target, 'SliceNormal', (1, 0, 0, 0))
        enabled = expression(target, u.MaterialExpressionScalarParameter,
                             parameter_name='SliceEnabled', default_value=0)
        mask = custom(target, 'return lerp(1,step(0,dot(P-O,N)),Enable);',
                      {'P': world, 'O': origin, 'N': normal, 'Enable': enabled})
        assert E.connect_material_property(mask, '', u.MaterialProperty.MP_OPACITY_MASK)
        E.layout_material_expressions(target)
        E.recompile_material(target)
        assert u.EditorAssetLibrary.save_loaded_asset(target)
        created.append(target_path)
    mapping[source] = target

light_path = ROOT + '/M_LF_PortalFlashlight'
if u.EditorAssetLibrary.does_asset_exist(light_path):
    light_material = u.load_asset(light_path)
else:
    light_material = u.EditorAssetLibrary.duplicate_asset('/Game/House/Flashlight/M_LF_Flashlight', light_path)
    assert light_material
    original = E.get_material_property_input_node(light_material, u.MaterialProperty.MP_EMISSIVE_COLOR)
    original_pin = E.get_material_property_input_node_output_name(light_material, u.MaterialProperty.MP_EMISSIVE_COLOR)
    assert original
    inputs = {'P': expression(light_material, u.MaterialExpressionWorldPosition)}
    for name, value in {'PortalOrigin': (0, 0, 0, 0), 'PortalNormal': (1, 0, 0, 0),
                        'PortalRight': (0, 1, 0, 0), 'PortalUp': (0, 0, 1, 0),
                        'LightOrigin': (-100, 0, 0, 0), 'PortalSize': (65, 115, 0, 0)}.items():
        inputs[name] = vector(light_material, name, value)
    aperture = custom(light_material, '''
float3 N=PortalNormal.xyz;
float3 D=P.xyz-LightOrigin.xyz;
float sourceSide=dot(LightOrigin.xyz-PortalOrigin.xyz,N);
float receiverSide=dot(P.xyz-PortalOrigin.xyz,N);
if(receiverSide<=0) return 0;
// A flashlight lens already through the opening emits directly into the destination.
if(sourceSide>=0) return 1;
float denom=dot(D,N);
if(denom<=.00001) return 0;
float t=-sourceSide/denom;
if(t<0 || t>1) return 0;
float3 Q=LightOrigin.xyz+D*t-PortalOrigin.xyz;
float2 uv=float2(dot(Q,PortalRight.xyz),dot(Q,PortalUp.xyz))/PortalSize.xy;
return 1-smoothstep(.96,1,dot(uv,uv));
''', inputs)
    product = expression(light_material, u.MaterialExpressionMultiply)
    assert E.connect_material_expressions(original, original_pin, product, 'A')
    assert E.connect_material_expressions(aperture, '', product, 'B')
    assert E.connect_material_property(product, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
    E.layout_material_expressions(light_material)
    E.recompile_material(light_material)
    assert u.EditorAssetLibrary.save_loaded_asset(light_material)
    created.append(light_path)

bound = False
if hasattr(u, 'InteriorPortalPresentation'):
    subsystem = u.get_editor_subsystem(u.EditorActorSubsystem)
    systems = [actor for actor in subsystem.get_all_level_actors() if isinstance(actor, u.InteriorPortalSystem)]
    assert len(systems) == 1
    presentation = systems[0].get_editor_property('player_presentation')
    presentation.set_editor_property('slice_materials', mapping)
    presentation.set_editor_property('flashlight_aperture_material', light_material)
    assert u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
    bound = True

result = {'created': created, 'slice_material_count': len(mapping), 'bound_to_map': bound}
with open(u.Paths.project_saved_dir() + 'PortalPlayerPresentationSetup.json', 'w') as stream:
    json.dump(result, stream, indent=2)
u.log('PORTAL_PLAYER_PRESENTATION_SETUP ' + json.dumps(result))
