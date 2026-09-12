"""Run with UE PythonScript commandlet; only authors the scoped interior portal assets/actors."""
import unreal as u
import json

ROOT = '/Game/SlayTheSpireDemo/Interior/Portals'
MAP = '/Game/House/L_Interior_LivingKitchen'
E = u.MaterialEditingLibrary
AT = u.AssetToolsHelpers.get_asset_tools()

def asset(name, cls, factory):
    path = ROOT + '/' + name
    return u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else AT.create_asset(name, ROOT, cls, factory)

def node(m, cls, **props):
    n = E.create_material_expression(m, cls)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n

def scalar(m, name, value):
    return node(m, u.MaterialExpressionScalarParameter, parameter_name=name, default_value=value)

def vector(m, name, value):
    return node(m, u.MaterialExpressionVectorParameter, parameter_name=name, default_value=u.LinearColor(*value))

def custom(m, code, inputs, typ=u.CustomMaterialOutputType.CMOT_FLOAT3):
    slots = []
    for name in inputs:
        slot = u.CustomInput(); slot.set_editor_property('input_name', name); slots.append(slot)
    n = node(m, u.MaterialExpressionCustom, code=code, output_type=typ, inputs=slots)
    for name, (source, pin) in inputs.items():
        assert E.connect_material_expressions(source, pin, n, name)
    return n

def output(n, prop):
    assert E.connect_material_property(n, '', prop)

placeholder = asset('RT_Portal_Default', u.TextureRenderTarget2D, u.TextureRenderTargetFactoryNew())
placeholder.set_editor_property('render_target_format', u.TextureRenderTargetFormat.RTF_RGBA16F)
placeholder.set_editor_property('size_x', 256); placeholder.set_editor_property('size_y', 256)
u.EditorAssetLibrary.save_loaded_asset(placeholder)

portal = asset('M_InteriorPortal', u.Material, u.MaterialFactoryNew())
E.delete_all_material_expressions(portal)
portal.set_editor_property('blend_mode', u.BlendMode.BLEND_MASKED)
portal.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
portal.set_editor_property('two_sided', False)
uv = node(portal, u.MaterialExpressionTextureCoordinate)
screen = node(portal, u.MaterialExpressionScreenPosition)
tex = node(portal, u.MaterialExpressionTextureSampleParameter2D, parameter_name='PortalView', texture=placeholder, sampler_type=u.MaterialSamplerType.SAMPLERTYPE_COLOR)
assert E.connect_material_expressions(screen, 'ViewportUV', tex, 'UVs')
time = node(portal, u.MaterialExpressionTime)
color = vector(portal, 'PortalColor', (.01, .25, 1, 1))
linked = scalar(portal, 'Linked', 0)
exposure = node(portal, u.MaterialExpressionEyeAdaptation)
shade = custom(portal, '''
float2 p=(UV-.5)*2;
float r=length(p);
float a=atan2(p.y,p.x);
float filament=.5+.5*sin(a*37-T*7+sin(a*13+T*3)*2.2);
float ripple=.5+.5*sin(a*19+T*4+r*80);
float ring=exp(-pow((r-.954)*45,2));
float core=exp(-pow((r-.950)*150,2));
float glow=ring*(5+filament*7+ripple*3)+core*14;
float3 dormant=C*(.025+.16*pow(saturate(1-r),2))*(.65+.35*sin(r*32-T*3+a*3));
float edge=smoothstep(.925,.967,r);
return lerp(lerp(dormant/max(Exposure,.000001),View,saturate(Linked)),(C*glow+core*2)/max(Exposure,.000001),edge);
''', {'UV':(uv,''), 'View':(tex,'RGB'), 'T':(time,''), 'C':(color,'RGB'), 'Linked':(linked,''), 'Exposure':(exposure,'')})
output(shade, u.MaterialProperty.MP_EMISSIVE_COLOR)
mask = custom(portal, 'return 1-step(.995,length((UV-.5)*2));', {'UV':(uv,'')}, u.CustomMaterialOutputType.CMOT_FLOAT1)
output(mask,u.MaterialProperty.MP_OPACITY_MASK)

cube_material = asset('M_PortalTestCube',u.Material,u.MaterialFactoryNew())
E.delete_all_material_expressions(cube_material)
cube_material.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED)
cube_uv=node(cube_material,u.MaterialExpressionTextureCoordinate)
cube_color=custom(cube_material,'''float2 p=abs(UV-.5)*2; float edge=step(.76,max(p.x,p.y)); float circle=1-smoothstep(.24,.28,length(UV-.5)); return lerp(lerp(float3(.68,.73,.77),float3(.055,.075,.095),edge),float3(.08,.35,.55),circle);''',{'UV':(cube_uv,'')})
output(cube_color,u.MaterialProperty.MP_BASE_COLOR)
output(scalar(cube_material,'Roughness',.38),u.MaterialProperty.MP_ROUGHNESS)
world=node(cube_material,u.MaterialExpressionWorldPosition)
origin=vector(cube_material,'SliceOrigin',(0,0,0,0))
normal=vector(cube_material,'SliceNormal',(1,0,0,0))
enabled=scalar(cube_material,'SliceEnabled',0)
slice_mask=custom(cube_material,'return lerp(1,step(0,dot(P-O,N)),Enable);',{'P':(world,''),'O':(origin,'RGB'),'N':(normal,'RGB'),'Enable':(enabled,'')},u.CustomMaterialOutputType.CMOT_FLOAT1)
output(slice_mask,u.MaterialProperty.MP_OPACITY_MASK)

panel_material=asset('M_PortalSurface',u.Material,u.MaterialFactoryNew())
E.delete_all_material_expressions(panel_material)
output(node(panel_material,u.MaterialExpressionConstant3Vector,constant=u.LinearColor(.68,.7,.72,1)),u.MaterialProperty.MP_BASE_COLOR)
output(scalar(panel_material,'Roughness',.74),u.MaterialProperty.MP_ROUGHNESS)
for material in [portal,cube_material,panel_material]:
    E.layout_material_expressions(material); E.recompile_material(material)
    assert u.EditorAssetLibrary.save_loaded_asset(material)

level=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert level.load_level(MAP)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
allactors=actors.get_all_level_actors()
by_label={a.get_actor_label():a for a in allactors}
def actor(name,cls,loc,rotation=None):
    a=by_label.get(name)
    if a is None:
        a=actors.spawn_actor_from_class(cls,u.Vector(*loc),rotation or u.Rotator())
        a.set_actor_label(name); a.set_folder_path('Interior/Portals')
    return a

panel=actor('Portal_DemonstrationPanel',u.StaticMeshActor,(1300,900,120))
panel.set_actor_scale3d(u.Vector(2.2,.12,3))
panel.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'))
panel.static_mesh_component.set_material(0,panel_material)
panel.static_mesh_component.set_collision_profile_name('BlockAll')
blue=actor('Portal_Blue',u.InteriorPortal,(1300,893.4,105),u.Rotator(yaw=-90))
orange=actor('Portal_Orange',u.InteriorPortal,(1789.4,1200,105),u.Rotator(yaw=180))
east=by_label['LivingKitchenLoop_TurnLeg_Wall_East']
for endpoint,support,tint in [(blue,panel.static_mesh_component,(.01,.25,1,1)),(orange,east.static_mesh_component,(1,.15,.008,1))]:
    endpoint.get_component_by_class(u.StaticMeshComponent).set_relative_rotation(u.Rotator(yaw=90,roll=-90),False,True)
    endpoint.set_editor_property('portal_material',portal)
    endpoint.set_editor_property('half_width',65)
    endpoint.set_editor_property('half_height',115)
    endpoint.set_editor_property('support',support)
    endpoint.set_editor_property('portal_color',u.LinearColor(*tint))
    endpoint.set_editor_property('placed',True)

cube=actor('Portal_TestCube',u.StaticMeshActor,(1420,560,45))
cube.set_actor_scale3d(u.Vector(.4,.4,.4))
cube.static_mesh_component.set_mobility(u.ComponentMobility.MOVABLE)
cube.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'))
cube.static_mesh_component.set_material(0,cube_material)
cube.static_mesh_component.set_collision_profile_name('PhysicsActor')
cube.static_mesh_component.set_simulate_physics(True)
cube.static_mesh_component.set_mass_override_in_kg('',12,True)

system=actor('Interior_PortalSystem',u.InteriorPortalSystem,(1300,750,0))
system.set_editor_property('blue_portal',blue)
system.set_editor_property('orange_portal',orange)
surfaces=[panel.static_mesh_component]
for a in allactors:
    name=a.get_actor_label()
    if isinstance(a,u.StaticMeshActor) and (name in ['West_Wall','East_Wall','South_Wall_Left','South_Wall_Right','Ceiling'] or name.startswith('LivingKitchenLoop_') and any(part in name for part in ['Wall','Floor','Ceiling'])):
        surfaces.append(a.static_mesh_component)
system.set_editor_property('portal_surfaces',surfaces)
system.set_editor_property('physics_travellers',[cube.static_mesh_component])
system.set_editor_property('recursion_depth',3)
system.set_editor_property('resolution_scale',.75)
assert level.save_current_level()
manifest={'map':MAP,'system':system.get_path_name(),'blue':blue.get_path_name(),'orange':orange.get_path_name(),'surfaces':[s.get_path_name() for s in surfaces],'cube':cube.get_path_name(),'materials':[m.get_path_name() for m in [portal,cube_material,panel_material]]}
with open(u.Paths.project_saved_dir()+'InteriorPortalsSetup.json','w') as f: json.dump(manifest,f,indent=2)
print('PORTALS_SETUP_SAVED',json.dumps(manifest))
