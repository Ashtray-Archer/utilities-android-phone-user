#ifndef PB_BINDER_FLAGS_H
#define PB_BINDER_FLAGS_H

/* libbinder IPCThreadState::transact adds TF_ACCEPT_FDS (0x10) before
 * dispatching Java Binder.onTransact. It describes reply transport, not an
 * authorization to carry descriptors in this request. Each owner separately
 * checks Parcel.hasFileDescriptors and the exact private request schema.
 * AOSP frameworks/native e59fde00320dcb3a8c927eaa84ab5d9fed003164,
 * libs/binder/IPCThreadState.cpp. Android's BinderProxy also adds
 * FLAG_COLLECT_NOTED_APP_OPS (0x02) to synchronous calls when listening for
 * noted operations; it does not change RPC direction or descriptor authority.
 * AOSP frameworks/base 5d412489475694286e3b3c43e74f42ef21ebc4de,
 * core/java/android/os/{BinderProxy,IBinder}.java. It is not added to oneway
 * calls. Reject all other unexpected flag bits. */
static inline int pb_binder_synchronous(int flags) {
    return (flags & ~0x12) == 0;
}

static inline int pb_binder_oneway(int flags) {
    return (flags & ~0x10) == 1;
}

#endif
