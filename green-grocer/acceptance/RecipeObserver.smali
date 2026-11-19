.class public Lacceptance/RecipeObserver;
.super Landroid/app/Instrumentation;
.field private automation:Landroid/app/UiAutomation;
.field private activity:Landroid/app/Activity;
.field private static down_time:J

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Landroid/app/Instrumentation;-><init>()V
    return-void
.end method

.method private static fail(Ljava/lang/String;)V
    .registers 2
    new-instance v0, Ljava/lang/IllegalStateException;
    invoke-direct {v0, p0}, Ljava/lang/IllegalStateException;-><init>(Ljava/lang/String;)V
    throw v0
.end method

.method public static read(Ljava/io/InputStream;)Ljava/lang/String;
    .registers 7
    new-instance v0, Ljava/io/ByteArrayOutputStream;
    invoke-direct {v0}, Ljava/io/ByteArrayOutputStream;-><init>()V
    const/16 v1, 4096
    new-array v1, v1, [B
  :again
    invoke-virtual {p0, v1}, Ljava/io/InputStream;->read([B)I
    move-result v2
    if-ltz v2, :done
    const/4 v3, 0
    invoke-virtual {v0, v1, v3, v2}, Ljava/io/ByteArrayOutputStream;->write([BII)V
    goto :again
  :done
    invoke-virtual {p0}, Ljava/io/InputStream;->close()V
    invoke-virtual {v0}, Ljava/io/ByteArrayOutputStream;->toByteArray()[B
    move-result-object v0
    new-instance v1, Ljava/lang/String;
    const-string v2, "UTF-8"
    invoke-direct {v1, v0, v2}, Ljava/lang/String;-><init>([BLjava/lang/String;)V
    return-object v1
.end method

.method private asset(Ljava/lang/String;)Ljava/lang/String;
    .registers 4
    invoke-virtual {p0}, Landroid/app/Instrumentation;->getContext()Landroid/content/Context;
    move-result-object v0
    invoke-virtual {v0}, Landroid/content/Context;->getAssets()Landroid/content/res/AssetManager;
    move-result-object v0
    invoke-virtual {v0, p1}, Landroid/content/res/AssetManager;->open(Ljava/lang/String;)Ljava/io/InputStream;
    move-result-object v0
    invoke-static {v0}, Lacceptance/RecipeObserver;->read(Ljava/io/InputStream;)Ljava/lang/String;
    move-result-object v0
    return-object v0
.end method

.method private shell(Ljava/lang/String;)Ljava/lang/String;
    .registers 5
    iget-object v0, p0, Lacceptance/RecipeObserver;->automation:Landroid/app/UiAutomation;
    invoke-virtual {v0, p1}, Landroid/app/UiAutomation;->executeShellCommand(Ljava/lang/String;)Landroid/os/ParcelFileDescriptor;
    move-result-object v0
    new-instance v1, Landroid/os/ParcelFileDescriptor$AutoCloseInputStream;
    invoke-direct {v1, v0}, Landroid/os/ParcelFileDescriptor$AutoCloseInputStream;-><init>(Landroid/os/ParcelFileDescriptor;)V
    invoke-static {v1}, Lacceptance/RecipeObserver;->read(Ljava/io/InputStream;)Ljava/lang/String;
    move-result-object v0
    return-object v0
.end method

.method public static hash(Ljava/lang/String;)Ljava/lang/String;
    .registers 4
    invoke-virtual {p0}, Ljava/lang/String;->hashCode()I
    move-result v0
    invoke-static {v0}, Ljava/lang/Integer;->toHexString(I)Ljava/lang/String;
    move-result-object v0
  :pad
    invoke-virtual {v0}, Ljava/lang/String;->length()I
    move-result v1
    const/16 v2, 8
    if-ge v1, v2, :done
    const-string v1, "0"
    invoke-virtual {v1, v0}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    goto :pad
  :done
    const-string v1, "hash="
    invoke-virtual {v1, v0}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    return-object v0
.end method

.method private clear()V
    .registers 2
    const-string v0, "logcat -c"
    invoke-direct {p0, v0}, Lacceptance/RecipeObserver;->shell(Ljava/lang/String;)Ljava/lang/String;
    return-void
.end method

.method private expect(Ljava/lang/String;)V
    .registers 12
    new-instance v0, Ljava/lang/StringBuilder;
    invoke-direct {v0}, Ljava/lang/StringBuilder;-><init>()V
    const-string v1, "expected/"
    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v0, p1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    const-string v1, ".view"
    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v0}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;
    move-result-object v0
    invoke-direct {p0, v0}, Lacceptance/RecipeObserver;->asset(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    invoke-static {v0}, Lacceptance/RecipeObserver;->hash(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    invoke-static {}, Landroid/os/SystemClock;->uptimeMillis()J
    move-result-wide v2
    const-wide/32 v4, 60000
    add-long/2addr v2, v4
  :again
    const-string v1, "logcat -d -v raw -s GGRecipeView:I GGRecipeBounds:I"
    invoke-direct {p0, v1}, Lacceptance/RecipeObserver;->shell(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v1
    invoke-virtual {v1, v0}, Ljava/lang/String;->contains(Ljava/lang/CharSequence;)Z
    move-result v6
    if-eqz v6, :wait
    const-string v6, "control=0 x="
    invoke-virtual {v1, v6}, Ljava/lang/String;->contains(Ljava/lang/CharSequence;)Z
    move-result v6
    if-nez v6, :passed
  :wait
    invoke-static {}, Landroid/os/SystemClock;->uptimeMillis()J
    move-result-wide v4
    cmp-long v6, v4, v2
    if-ltz v6, :sleep
    const-string v0, "FAIL expected view "
    invoke-virtual {v0, p1}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    invoke-static {v0}, Lacceptance/RecipeObserver;->fail(Ljava/lang/String;)V
  :sleep
    const-wide/16 v4, 200
    invoke-static {v4, v5}, Landroid/os/SystemClock;->sleep(J)V
    goto :again
  :passed
    const-wide/16 v4, 250
    invoke-static {v4, v5}, Landroid/os/SystemClock;->sleep(J)V
    invoke-direct {p0, p1}, Lacceptance/RecipeObserver;->screenshot(Ljava/lang/String;)V
    new-instance v7, Landroid/os/Bundle;
    invoke-direct {v7}, Landroid/os/Bundle;-><init>()V
    const-string v8, "phase"
    invoke-virtual {v7, v8, p1}, Landroid/os/Bundle;->putString(Ljava/lang/String;Ljava/lang/String;)V
    const-string v8, "matched_view"
    invoke-virtual {v7, v8, v0}, Landroid/os/Bundle;->putString(Ljava/lang/String;Ljava/lang/String;)V
    const/4 v8, 1
    invoke-virtual {p0, v8, v7}, Landroid/app/Instrumentation;->sendStatus(ILandroid/os/Bundle;)V
    return-void
.end method

.method private wait_picker()V
    .registers 8
    invoke-static {}, Landroid/os/SystemClock;->uptimeMillis()J
    move-result-wide v0
    const-wide/16 v2, 20000
    add-long/2addr v0, v2
  :again
    iget-object v4, p0, Lacceptance/RecipeObserver;->automation:Landroid/app/UiAutomation;
    invoke-virtual {v4}, Landroid/app/UiAutomation;->getRootInActiveWindow()Landroid/view/accessibility/AccessibilityNodeInfo;
    move-result-object v4
    if-eqz v4, :wait
    invoke-virtual {v4}, Landroid/view/accessibility/AccessibilityNodeInfo;->getPackageName()Ljava/lang/CharSequence;
    move-result-object v4
    if-eqz v4, :wait
    invoke-virtual {v4}, Ljava/lang/Object;->toString()Ljava/lang/String;
    move-result-object v4
    const-string v5, ".documentsui"
    invoke-virtual {v4, v5}, Ljava/lang/String;->endsWith(Ljava/lang/String;)Z
    move-result v5
    if-nez v5, :done
  :wait
    invoke-static {}, Landroid/os/SystemClock;->uptimeMillis()J
    move-result-wide v2
    cmp-long v4, v2, v0
    if-ltz v4, :sleep
    const-string v4, "FAIL system document picker did not open"
    invoke-static {v4}, Lacceptance/RecipeObserver;->fail(Ljava/lang/String;)V
  :sleep
    const-wide/16 v2, 200
    invoke-static {v2, v3}, Landroid/os/SystemClock;->sleep(J)V
    goto :again
  :done
    return-void
.end method

.method private screenshot(Ljava/lang/String;)V
    .registers 8
    iget-object v0, p0, Lacceptance/RecipeObserver;->automation:Landroid/app/UiAutomation;
    invoke-virtual {v0}, Landroid/app/UiAutomation;->takeScreenshot()Landroid/graphics/Bitmap;
    move-result-object v0
    if-nez v0, :image
    const-string v4, "FAIL installed recipe screenshot"
    invoke-static {v4}, Lacceptance/RecipeObserver;->fail(Ljava/lang/String;)V
  :image
    invoke-virtual {p0}, Landroid/app/Instrumentation;->getTargetContext()Landroid/content/Context;
    move-result-object v1
    invoke-virtual {v1}, Landroid/content/Context;->getCacheDir()Ljava/io/File;
    move-result-object v1
    const-string v2, "gg-"
    invoke-virtual {v2, p1}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v2
    const-string v3, ".png"
    invoke-virtual {v2, v3}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v2
    new-instance v3, Ljava/io/File;
    invoke-direct {v3, v1, v2}, Ljava/io/File;-><init>(Ljava/io/File;Ljava/lang/String;)V
    new-instance v1, Ljava/io/FileOutputStream;
    invoke-direct {v1, v3}, Ljava/io/FileOutputStream;-><init>(Ljava/io/File;)V
    sget-object v2, Landroid/graphics/Bitmap$CompressFormat;->PNG:Landroid/graphics/Bitmap$CompressFormat;
    const/16 v3, 100
    invoke-virtual {v0, v2, v3, v1}, Landroid/graphics/Bitmap;->compress(Landroid/graphics/Bitmap$CompressFormat;ILjava/io/OutputStream;)Z
    move-result v2
    invoke-virtual {v1}, Ljava/io/FileOutputStream;->close()V
    invoke-virtual {v0}, Landroid/graphics/Bitmap;->recycle()V
    if-nez v2, :done
    const-string v0, "FAIL recipe screenshot encoding"
    invoke-static {v0}, Lacceptance/RecipeObserver;->fail(Ljava/lang/String;)V
  :done
    return-void
.end method

.method private static inject(Landroid/app/UiAutomation;IFF)V
    .registers 12
    invoke-static {}, Landroid/os/SystemClock;->uptimeMillis()J
    move-result-wide v2
    if-nez p1, :retained
    sput-wide v2, Lacceptance/RecipeObserver;->down_time:J
  :retained
    sget-wide v0, Lacceptance/RecipeObserver;->down_time:J
    move v4, p1
    move v5, p2
    move v6, p3
    const/4 v7, 0
    invoke-static/range {v0 .. v7}, Landroid/view/MotionEvent;->obtain(JJIFFI)Landroid/view/MotionEvent;
    move-result-object v0
    const/16 v1, 0x1002
    invoke-virtual {v0, v1}, Landroid/view/MotionEvent;->setSource(I)V
    const/4 v1, 1
    invoke-virtual {p0, v0, v1}, Landroid/app/UiAutomation;->injectInputEvent(Landroid/view/InputEvent;Z)Z
    move-result v1
    invoke-virtual {v0}, Landroid/view/MotionEvent;->recycle()V
    if-nez v1, :done
    const-string v0, "FAIL native recipe touch injection"
    invoke-static {v0}, Lacceptance/RecipeObserver;->fail(Ljava/lang/String;)V
  :done
    return-void
.end method

.method private tap(II)V
    .registers 12
    new-instance v0, Ljava/lang/StringBuilder;
    invoke-direct {v0}, Ljava/lang/StringBuilder;-><init>()V
    const-string v1, "row="
    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v0, p1}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;
    const-string v1, " control="
    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v0, p2}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;
    const-string v1, " x=([0-9]+) y=([0-9]+)"
    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v0}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;
    move-result-object v0
    invoke-static {v0}, Ljava/util/regex/Pattern;->compile(Ljava/lang/String;)Ljava/util/regex/Pattern;
    move-result-object v0
    const-string v1, "logcat -d -v raw -s GGRecipeBounds:I"
    invoke-direct {p0, v1}, Lacceptance/RecipeObserver;->shell(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v1
    invoke-virtual {v0, v1}, Ljava/util/regex/Pattern;->matcher(Ljava/lang/CharSequence;)Ljava/util/regex/Matcher;
    move-result-object v0
    const/4 v1, -1
    const/4 v2, -1
  :again
    invoke-virtual {v0}, Ljava/util/regex/Matcher;->find()Z
    move-result v3
    if-eqz v3, :coordinates
    const/4 v3, 1
    invoke-virtual {v0, v3}, Ljava/util/regex/Matcher;->group(I)Ljava/lang/String;
    move-result-object v1
    invoke-static {v1}, Ljava/lang/Integer;->parseInt(Ljava/lang/String;)I
    move-result v1
    const/4 v3, 2
    invoke-virtual {v0, v3}, Ljava/util/regex/Matcher;->group(I)Ljava/lang/String;
    move-result-object v2
    invoke-static {v2}, Ljava/lang/Integer;->parseInt(Ljava/lang/String;)I
    move-result v2
    goto :again
  :coordinates
    if-ltz v1, :missing
    if-gez v2, :offset
  :missing
    const-string v0, "FAIL missing native recipe touch bounds"
    invoke-static {v0}, Lacceptance/RecipeObserver;->fail(Ljava/lang/String;)V
  :offset
    iget-object v0, p0, Lacceptance/RecipeObserver;->activity:Landroid/app/Activity;
    invoke-virtual {v0}, Landroid/app/Activity;->getWindow()Landroid/view/Window;
    move-result-object v0
    invoke-virtual {v0}, Landroid/view/Window;->getDecorView()Landroid/view/View;
    move-result-object v0
    const/4 v3, 2
    new-array v3, v3, [I
    invoke-virtual {v0, v3}, Landroid/view/View;->getLocationOnScreen([I)V
    const/4 v4, 0
    aget v0, v3, v4
    add-int/2addr v1, v0
    const/4 v4, 1
    aget v0, v3, v4
    add-int/2addr v2, v0
    int-to-float v1, v1
    int-to-float v2, v2
    invoke-direct {p0}, Lacceptance/RecipeObserver;->clear()V
    iget-object v0, p0, Lacceptance/RecipeObserver;->automation:Landroid/app/UiAutomation;
    const/4 v3, 0
    invoke-static {v0, v3, v1, v2}, Lacceptance/RecipeObserver;->inject(Landroid/app/UiAutomation;IFF)V
    const-wide/16 v4, 60
    invoke-static {v4, v5}, Landroid/os/SystemClock;->sleep(J)V
    const/4 v3, 1
    invoke-static {v0, v3, v1, v2}, Lacceptance/RecipeObserver;->inject(Landroid/app/UiAutomation;IFF)V
    return-void
.end method

.method private deliver()V
    .registers 9
    const-string v0, "fixture.recipe"
    invoke-direct {p0, v0}, Lacceptance/RecipeObserver;->asset(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    invoke-virtual {p0}, Landroid/app/Instrumentation;->getTargetContext()Landroid/content/Context;
    move-result-object v1
    invoke-virtual {v1}, Landroid/content/Context;->getCacheDir()Ljava/io/File;
    move-result-object v1
    new-instance v2, Ljava/io/File;
    const-string v3, "observer-input.recipe"
    invoke-direct {v2, v1, v3}, Ljava/io/File;-><init>(Ljava/io/File;Ljava/lang/String;)V
    new-instance v1, Ljava/io/FileOutputStream;
    invoke-direct {v1, v2}, Ljava/io/FileOutputStream;-><init>(Ljava/io/File;)V
    const-string v3, "UTF-8"
    invoke-virtual {v0, v3}, Ljava/lang/String;->getBytes(Ljava/lang/String;)[B
    move-result-object v0
    invoke-virtual {v1, v0}, Ljava/io/FileOutputStream;->write([B)V
    invoke-virtual {v1}, Ljava/io/FileOutputStream;->close()V
    invoke-static {v2}, Landroid/net/Uri;->fromFile(Ljava/io/File;)Landroid/net/Uri;
    move-result-object v0
    new-instance v1, Landroid/content/Intent;
    invoke-direct {v1}, Landroid/content/Intent;-><init>()V
    invoke-virtual {v1, v0}, Landroid/content/Intent;->setData(Landroid/net/Uri;)Landroid/content/Intent;
    new-instance v0, Lacceptance/RecipeDeliver;
    iget-object v2, p0, Lacceptance/RecipeObserver;->activity:Landroid/app/Activity;
    invoke-direct {v0, v2, v1}, Lacceptance/RecipeDeliver;-><init>(Landroid/app/Activity;Landroid/content/Intent;)V
    invoke-virtual {p0, v0}, Landroid/app/Instrumentation;->runOnMainSync(Ljava/lang/Runnable;)V
    iget-object v0, p0, Lacceptance/RecipeObserver;->automation:Landroid/app/UiAutomation;
    const/4 v1, 1
    invoke-virtual {v0, v1}, Landroid/app/UiAutomation;->performGlobalAction(I)Z
    return-void
.end method

.method public onCreate(Landroid/os/Bundle;)V
    .registers 3
    invoke-super {p0, p1}, Landroid/app/Instrumentation;->onCreate(Landroid/os/Bundle;)V
    invoke-virtual {p0}, Landroid/app/Instrumentation;->start()V
    return-void
.end method

.method public onStart()V
    .registers 9
  :start
    invoke-virtual {p0}, Landroid/app/Instrumentation;->getUiAutomation()Landroid/app/UiAutomation;
    move-result-object v0
    iput-object v0, p0, Lacceptance/RecipeObserver;->automation:Landroid/app/UiAutomation;
    invoke-direct {p0}, Lacceptance/RecipeObserver;->clear()V
    new-instance v0, Landroid/content/Intent;
    invoke-direct {v0}, Landroid/content/Intent;-><init>()V
    const-string v1, "org.isomorphisms.greengrocer"
    const-string v2, "org.isomorphisms.greengrocer.RecipeActivity"
    invoke-virtual {v0, v1, v2}, Landroid/content/Intent;->setClassName(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;
    const/high16 v1, 0x10000000
    invoke-virtual {v0, v1}, Landroid/content/Intent;->addFlags(I)Landroid/content/Intent;
    invoke-virtual {p0, v0}, Landroid/app/Instrumentation;->startActivitySync(Landroid/content/Intent;)Landroid/app/Activity;
    move-result-object v0
    iput-object v0, p0, Lacceptance/RecipeObserver;->activity:Landroid/app/Activity;
    const-string v0, "empty"
    invoke-direct {p0, v0}, Lacceptance/RecipeObserver;->expect(Ljava/lang/String;)V
    const/4 v0, 0
    invoke-direct {p0, v0, v0}, Lacceptance/RecipeObserver;->tap(II)V
    invoke-direct {p0}, Lacceptance/RecipeObserver;->wait_picker()V
    invoke-direct {p0}, Lacceptance/RecipeObserver;->deliver()V
    const-string v0, "imported"
    invoke-direct {p0, v0}, Lacceptance/RecipeObserver;->expect(Ljava/lang/String;)V
    const/4 v0, 1
    const/4 v1, 0
    invoke-direct {p0, v0, v1}, Lacceptance/RecipeObserver;->tap(II)V
    const-string v0, "selected"
    invoke-direct {p0, v0}, Lacceptance/RecipeObserver;->expect(Ljava/lang/String;)V
    const/4 v0, 2
    const/4 v1, 1
    invoke-direct {p0, v0, v1}, Lacceptance/RecipeObserver;->tap(II)V
    const-string v0, "ingredients"
    invoke-direct {p0, v0}, Lacceptance/RecipeObserver;->expect(Ljava/lang/String;)V
    const/4 v0, 0
    const/4 v1, 1
    invoke-direct {p0, v0, v1}, Lacceptance/RecipeObserver;->tap(II)V
    const-string v0, "scaled"
    invoke-direct {p0, v0}, Lacceptance/RecipeObserver;->expect(Ljava/lang/String;)V
    const/4 v0, 1
    invoke-direct {p0, v0, v0}, Lacceptance/RecipeObserver;->tap(II)V
    const-string v0, "pantry"
    invoke-direct {p0, v0}, Lacceptance/RecipeObserver;->expect(Ljava/lang/String;)V
    const/4 v0, 3
    const/4 v1, 1
    invoke-direct {p0, v0, v1}, Lacceptance/RecipeObserver;->tap(II)V
    const-string v0, "review"
    invoke-direct {p0, v0}, Lacceptance/RecipeObserver;->expect(Ljava/lang/String;)V
    const/4 v0, 1
    const/4 v1, 0
    invoke-direct {p0, v0, v1}, Lacceptance/RecipeObserver;->tap(II)V
    const-string v0, "refused"
    invoke-direct {p0, v0}, Lacceptance/RecipeObserver;->expect(Ljava/lang/String;)V
    invoke-direct {p0}, Lacceptance/RecipeObserver;->clear()V
    const/4 v0, 4
    invoke-virtual {p0, v0}, Landroid/app/Instrumentation;->sendKeyDownUpSync(I)V
    const-string v0, "back"
    invoke-direct {p0, v0}, Lacceptance/RecipeObserver;->expect(Ljava/lang/String;)V
    const/4 v0, 2
    const/4 v1, 0
    invoke-direct {p0, v0, v1}, Lacceptance/RecipeObserver;->tap(II)V
    const-string v0, "recipes"
    invoke-direct {p0, v0}, Lacceptance/RecipeObserver;->expect(Ljava/lang/String;)V
    const/4 v0, 1
    invoke-direct {p0, v0, v0}, Lacceptance/RecipeObserver;->tap(II)V
    const-string v0, "details"
    invoke-direct {p0, v0}, Lacceptance/RecipeObserver;->expect(Ljava/lang/String;)V
    const/4 v0, 0
    invoke-direct {p0, v0, v0}, Lacceptance/RecipeObserver;->tap(II)V
    const-string v0, "saved"
    invoke-direct {p0, v0}, Lacceptance/RecipeObserver;->expect(Ljava/lang/String;)V
    new-instance v0, Landroid/os/Bundle;
    invoke-direct {v0}, Landroid/os/Bundle;-><init>()V
    const-string v1, "stream"
    const-string v2, "PASS installed native recipe import callback, selection, servings, pantry, review refusal, back and save\n"
    invoke-virtual {v0, v1, v2}, Landroid/os/Bundle;->putString(Ljava/lang/String;Ljava/lang/String;)V
    const/4 v1, -1
    invoke-virtual {p0, v1, v0}, Landroid/app/Instrumentation;->finish(ILandroid/os/Bundle;)V
  :end
    return-void
    .catch Ljava/lang/Throwable; {:start .. :end} :failed
  :failed
    move-exception v0
    const-string v1, "GGRecipeObserver"
    const-string v2, "Installed native recipe observation failed"
    invoke-static {v1, v2, v0}, Landroid/util/Log;->e(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I
    invoke-virtual {v0}, Ljava/lang/Throwable;->getMessage()Ljava/lang/String;
    move-result-object v2
    new-instance v0, Landroid/os/Bundle;
    invoke-direct {v0}, Landroid/os/Bundle;-><init>()V
    const-string v1, "stream"
    invoke-virtual {v0, v1, v2}, Landroid/os/Bundle;->putString(Ljava/lang/String;Ljava/lang/String;)V
    const/4 v1, 0
    invoke-virtual {p0, v1, v0}, Landroid/app/Instrumentation;->finish(ILandroid/os/Bundle;)V
    return-void
.end method
