# Math Characters

A deliberately restricted Android input method for entering a small math-oriented character set without opening a full Unicode browser.

Current keys:

- arrows: `← ↑ ↓ → ↔ ↦ ⇒`
- number sets and infinity: `ℂ ℝ ℚ ℤ ℕ ∞`
- basis / imaginary letters: `i j k`
- ordinary digits: `0–9`, decimal point, `+`, and Unicode minus `−`
- superscripts: `⁰ ¹ ² ³ ⁴ ⁵ ⁶ ⁷ ⁸ ⁹ ⁺ ⁻ ⁼ ⁽ ⁾ ⁱ ʲ ᵏ ˡ ⁿ`
- subscripts: `₀ ₁ ₂ ₃ ₄ ₅ ₆ ₇ ₈ ₉ ₊ ₋ ₌ ₍ ₎ ᵢ ⱼ ₖ ₗ`
- space, backspace, enter, and a key for switching to the next installed keyboard

The superscript and subscript rows intentionally include the non-digit forms already used by the repository's broader Unicode picker; in particular, superscript `ⁿ` is part of this compact IME.

The larger Unicode picker also carries extended arrows, punctuation, programming symbols, and other pages. Those remain separate rather than turning this keyboard into a full Unicode browser.

Install the APK, open **Math Characters**, enable the input method in Android settings, then use **Choose keyboard now** to select it.

The pull-request workflow builds the debug APK and uploads it as `math-characters-debug-apk`.
