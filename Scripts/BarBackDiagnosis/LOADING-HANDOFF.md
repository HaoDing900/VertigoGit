# Loading and street intro handoff

Street entry K2Node_CallFunction_37 now invokes PlayEntrySequenceWhenReady. With a loading cover present, the player evaluates the first shot and pauses, allowing camera/scene readiness checks to settle. TickFadeOut resumes it when the loading cover starts revealing the destination. With no cover it plays immediately. No fixed extra waiting time is added to the intro.

Travel now asynchronously preloads the destination package while the old viewport remains active, retains the destination world until handoff, and then performs OpenLevel. Cancellation invalidates stale callbacks and releases preload references. Unknown short map names fall back to the original travel path.

The existing CircularThrobber is already animated in Slate. Preloading reduces the main-thread blocking period; final PIE world teardown/initialization can still briefly stall the viewport. This does not promise fully asynchronous PIE travel or zero stutter. Standalone/package MoviePlayer remains the independent loading display path.

Build: VertigoEditor Win64 Development passed.
Tests: -run=VTGLoadingScreen -VerifyEntryHandoff; 10 checks passed in loading-handoff-tests-final.log, including an actual async /Engine/Maps/Entry preload and cancellation.
Asset apply: -run=VTGLoadingScreen -RepairEntryHandoff.
Asset reload verification: -run=VTGLoadingScreen -RepairEntryHandoff -VerifyOnly.
Full L2Bar to L2_StreetOutsideBar visual playthrough and packaged-build spinner verification remain to be performed.

Asset backup: BPLM_L2StreetBarFight.before-loading-handoff.uasset.
