# Compact math keyboard sample

This is a deliberately small Android input method with twenty-one character keys:

```text
ℕ ℤ ℚ ℝ ℂ
= ≠ ≟ ∧ ← →
λ π ∂ ∫ ∞
ⁿ ᵢ ² − –
```

The final row keeps mathematical minus `−` and en dash `–` visibly distinct.

It also provides space, backspace, enter, and a keyboard-picker key. It is a
real Android keyboard, not the standalone Programmer's Unicode Picker in
`math-characters/`.

The package is `com.ashtrayarcher.mathsamplekeyboard`. Android input methods
must inherit from the platform `InputMethodService`, so this sample necessarily
contains DEX bytecode. It uses no network access and requests no Android
permissions other than the system-bound input-method service permission.

The pull-request workflow compiles the exact twenty-one-key contract, builds and signs the
APK with the repository's persistent public test signer, installs the same APK
twice without uninstalling, enables the keyboard in an emulator, confirms all
twenty keys are visible, taps `λ`, and verifies that `λ` reaches an editable text
field. Those checks do not claim physical-phone execution.

After installing the APK, open **Compact Math Keyboard**, tap **Enable
keyboard**, enable it in Android settings, then tap **Choose keyboard**.
