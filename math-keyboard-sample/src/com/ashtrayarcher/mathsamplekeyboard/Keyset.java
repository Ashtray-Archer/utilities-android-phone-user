package com.ashtrayarcher.mathsamplekeyboard;

import java.util.Arrays;

final class Keyset {
    static final String[][] ROWS = {
        {"ℕ", "ℤ", "ℚ", "ℝ", "ℂ"},
        {"=", "≠", "≟", "∧", "→"},
        {"λ", "π", "∂", "∫", "∞"},
        {"ⁿ", "ᵢ", "²", "−", "–"}
    };

    static boolean isExact(String[][] candidate) {
        return Arrays.deepEquals(ROWS, candidate);
    }

    private Keyset() {}
}
