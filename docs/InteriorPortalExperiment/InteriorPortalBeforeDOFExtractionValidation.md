# Interior Portal — STEP 1B.6 Validation Handoff

Date: **2026-09-15**

Current implementation:

```text
796306522cb980fa83e948eec7677d565a531246
portal: add BeforeDOF lit scene color extraction spike

412cf1c1d8555cb23d7a456bf58d5bffe3ac43b6
docs(portal): document BeforeDOF extraction spike
```

State:

```text
STEP 1B.5 FULL VIEW-FAMILY SUBMISSION = PASS
STEP 1B.6 BEFORE-DOF EXTRACTION = IMPLEMENTED / NOT YET BUILT OR RUN
```

Build the editor target, run the same portal map in PIE/Game, keep a linked
portal visible, then execute:

```text
portal.RunFullViewFamilyBeforeDOFSpike
```

Expected artifacts:

```text
Saved/AutomationReports/PortalFullViewFamilyBeforeDOF_Final.png
Saved/AutomationReports/PortalFullViewFamilyBeforeDOF_Final.exr
Saved/AutomationReports/PortalFullViewFamilyBeforeDOF_SceneColor.png
Saved/AutomationReports/PortalFullViewFamilyBeforeDOF_SceneColor.exr
Saved/AutomationReports/PortalFullViewFamilyBeforeDOFSpike.json
```

First inspect the JSON. `beforeDOFCallbackExecuted` must be `true`. Then compare
`*_SceneColor.exr` with `*_Final.exr` and the earlier BaseColor CRP evidence.

If compilation fails, report the first C++ compiler error. If runtime asserts,
report the assertion and top Renderer stack frames. If it runs, provide the JSON
and preferably both `BeforeDOF_SceneColor.png/.exr` outputs.

Do not alter exposure multipliers, gamma or brightness to make the diagnostic
look correct. This gate is about extracting the correct renderer domain.
