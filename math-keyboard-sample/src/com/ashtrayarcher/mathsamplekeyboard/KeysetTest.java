package com.ashtrayarcher.mathsamplekeyboard;

public final class KeysetTest {
    public static void main(String[] arguments) {
        String[][] expected = {
            {"ℕ", "ℤ", "ℚ", "ℝ", "ℂ"},
            {"=", "≠", "≟", "∧", "→"},
            {"λ", "π", "∂", "∫", "∞"},
            {"ⁿ", "ᵢ", "²", "−", "–"}
        };
        if (!Keyset.isExact(expected)) {
            throw new AssertionError("exact twenty-key sample was rejected");
        }

        String[][] wrongDash = {
            {"ℕ", "ℤ", "ℚ", "ℝ", "ℂ"},
            {"=", "≠", "≟", "∧", "→"},
            {"λ", "π", "∂", "∫", "∞"},
            {"ⁿ", "ᵢ", "²", "−", "−"}
        };
        if (Keyset.isExact(wrongDash)) {
            throw new AssertionError("mathematical minus was accepted in place of en dash");
        }

        System.out.println("PASS exact compact key set with distinct minus and en dash");
    }
}
