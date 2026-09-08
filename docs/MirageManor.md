# Mirage Manor architectural assembly

Built in `/Game/House/L_Interior_LivingKitchen`, centered at world `(6000, 0, 0)` cm, east of the existing house. Actors are grouped under `House/MirageManor` in the Outliner.

This is an original Gothic manor assembly inspired by the requested atmosphere, not an exact reconstruction of a supplied reference. It includes a high central hall, two wings with upper galleries and stairs, pitched roofs, stained-glass facade modules, buttresses, chandeliers, a rear ceremonial chair and a lantern courtyard. Dragon_Rise assets are referenced directly without modifying their shared meshes. The existing house and its interaction controller remain in the map.

A tagged `PCG_HouseExclusion` TriggerBox covers the new manor and courtyard: X 4100..7900, Y -3200..1400, Z -200..1800 cm. The existing tag-based forest graph can subtract this area when moved here.

Validation: map saved successfully; PIE started and stopped with an entrance spawn override; entrance and hall visually inspected. A horizontal trace from `(6000,-1600,120)` to `(6000,700,120)` had no obstruction; a downward hall trace hit the floor at Z 11 cm. Full player walking/stair traversal and night appearance are not yet manually accepted. No C++ changes or rebuild were needed.

Remaining art needs for a more furnished period interior: Gothic wooden doors, antique bookcases, portrait paintings, heavy curtains, aged furniture and rose vegetation. Current upper galleries are a basic architectural assembly; functional doors and newly wired wall switches for manor lights are not included.
