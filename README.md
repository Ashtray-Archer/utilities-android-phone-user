# utilities-android-phone-user

Small Android utilities intended for direct use on a phone.

Each utility lives in its own folder and should remain independently buildable where practical.

[`accelerometer`](accelerometer) is a small live accelerometer viewer: a DEX-free native APK that displays the raw x/y/z acceleration stream and serves as the Android platform oracle for later Idriç ARM/Thumb ownership.

[`math-characters`](math-characters) is **Programmer's Unicode Picker**: an Idriç-owned, standalone copy/paste picker whose Android APK remains native, DEX-free, and separate from the system keyboard.

[`text-pad`](text-pad) records the product direction and phone user stories for a deliberately small plain-text editor. It is a design note rather than an implementation today.

[`android-clipboard`](android-clipboard) is the small reusable C/JNI boundary to Android's system clipboard for native utilities; it keeps application editing and execution semantics outside the framework bridge.
