# utilities-android-phone-user

Small Android utilities intended for direct use on a phone.

Each utility lives in its own folder and should remain independently buildable where practical.

[`accelerometer`](accelerometer) is a small live accelerometer viewer: a DEX-free native APK that displays the raw x/y/z acceleration stream and serves as the Android platform oracle for later Idriç ARM/Thumb ownership.

### Accelerometer 0.2.0 replay

<video controls muted loop src="https://github.com/Ashtray-Archer/utilities-android-phone-user/releases/download/accelerometer-native-v0.2.0/accelerometer-native-v0.2.0-replay.mp4"></video>

[![Recorded accelerometer replay](https://github.com/Ashtray-Archer/utilities-android-phone-user/releases/download/accelerometer-native-v0.2.0/accelerometer-native-v0.2.0-replay.gif)](https://github.com/Ashtray-Archer/utilities-android-phone-user/releases/download/accelerometer-native-v0.2.0/accelerometer-native-v0.2.0-replay.mp4)

[MP4 replay](https://github.com/Ashtray-Archer/utilities-android-phone-user/releases/download/accelerometer-native-v0.2.0/accelerometer-native-v0.2.0-replay.mp4) · [0.2.0 release](https://github.com/Ashtray-Archer/utilities-android-phone-user/releases/tag/accelerometer-native-v0.2.0)

The replay is reconstructed from a recorded retained-Float16 X/Y/Z sample sequence; it is a visualization of recorded sensor values, not a screen recording.

[`math-characters`](math-characters) is **Programmer's Unicode Picker**: an Idriç-owned, standalone copy/paste picker whose Android APK remains native, DEX-free, and separate from the system keyboard.

[`text-pad`](text-pad) records the product direction and phone user stories for a deliberately small plain-text editor. It is a design note rather than an implementation today.

[`android-clipboard`](android-clipboard) is the small reusable C/JNI boundary to Android's system clipboard for native utilities; it keeps application editing and execution semantics outside the framework bridge.

[`hardware`](hardware) is the phone-hardware experiment: a synthetic file-like `/hardware` namespace with a native Android sensor backend, a separate Termux:API path, and explicit probes for how far an ordinary process can reach below Android's stable APIs.
