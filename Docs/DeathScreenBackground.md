# Death screen background

Open `/Game/Blueprints/Widget/WBP_Death`. Under **Class Defaults → Death Screen → Background**, select the normal death artwork in **Default Death Image**. The full-screen `DeathBackgroundImage` layer sits behind the text/buttons and above the original blur. If no image is assigned, the original blurred background remains visible.

For a prison or other special outcome, use **Create Widget → WBP_Death** and set:

- **Use Other Death Image** = true.
- **Other Death Image** = the desired Texture2D.

Ordinary deaths need no override: the switch defaults to false. If the override is enabled but its image is empty, the standard image is used. Existing Create Widget nodes may need **Refresh Node** to display the new pins.

For an already-created widget, call **Set Death Background** with the switch and texture before/after Add to Viewport. Calling it with false returns to the default. Merely setting variables after the widget is visible requires this function to refresh the displayed image.

Use Texture2D artwork imported into the project. Choose your own default and prison images; no specific artwork has been selected automatically. The existing Retry/Main Menu buttons, mouse handling and retry shortcut remain in place.

Verification: `UnrealEditor-Cmd.exe Vertigo.uproject -run=VTGLoadingScreen -DeathBackground -Verify -unattended -nullrhi -nosound -nop4`.
