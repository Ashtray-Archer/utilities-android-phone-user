.class public Lacceptance/RecipeEditorObserver;
.super Lacceptance/RecipeObserver;
.field private blocked:Ljava/io/File;
.field private held:Ljava/io/File;

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Lacceptance/RecipeObserver;-><init>()V
    return-void
.end method

.method private tap(IILjava/lang/String;)V
    .registers 4
    invoke-virtual {p0, p1, p2}, Lacceptance/RecipeObserver;->editor_tap(II)V
    invoke-virtual {p0, p3}, Lacceptance/RecipeObserver;->editor_expect(Ljava/lang/String;)V
    return-void
.end method

.method private keyboard()V
    .registers 7
    invoke-static {}, Landroid/os/SystemClock;->uptimeMillis()J
    move-result-wide v0
    const-wide/16 v2, 30000
    add-long/2addr v0, v2
  :again
    const-string v4, "logcat -d -v raw -s GGRecipeIme:I"
    invoke-virtual {p0, v4}, Lacceptance/RecipeObserver;->editor_shell(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v4
    const-string v5, "SHOWN system-bound recipe input connection"
    invoke-virtual {v4, v5}, Ljava/lang/String;->contains(Ljava/lang/CharSequence;)Z
    move-result v4
    if-nez v4, :done
    invoke-static {}, Landroid/os/SystemClock;->uptimeMillis()J
    move-result-wide v2
    cmp-long v4, v2, v0
    if-ltz v4, :wait
    new-instance v4, Ljava/lang/IllegalStateException;
    const-string v5, "Test keyboard did not bind and show"
    invoke-direct {v4, v5}, Ljava/lang/IllegalStateException;-><init>(Ljava/lang/String;)V
    throw v4
  :wait
    const-wide/16 v2, 200
    invoke-static {v2, v3}, Landroid/os/SystemClock;->sleep(J)V
    goto :again
  :done
    new-instance v0, Landroid/os/Bundle;
    invoke-direct {v0}, Landroid/os/Bundle;-><init>()V
    const-string v1, "system_ime_phase"
    const-string v2, "SHOWN"
    invoke-virtual {v0, v1, v2}, Landroid/os/Bundle;->putString(Ljava/lang/String;Ljava/lang/String;)V
    const/4 v1, 1
    invoke-virtual {p0, v1, v0}, Landroid/app/Instrumentation;->sendStatus(ILandroid/os/Bundle;)V
    return-void
.end method

.method private ime(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V
    .registers 15
    new-instance v0, Ljava/lang/StringBuilder;
    invoke-direct {v0}, Ljava/lang/StringBuilder;-><init>()V
    const-string v1, "am broadcast -n org.isomorphisms.greengrocer.acceptance/acceptance.RecipeImeCommand --es phase "
    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v0, p1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    const-string v1, " --es operation "
    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v0, p2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    const-string v1, " --es text ime/"
    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v0, p3}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    const-string v1, ".txt --es expected ime/"
    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v0, p4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    const-string v1, ".txt --ei first "
    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v0, p5}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;
    const-string v1, " --ei last "
    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v0, p6}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;
    invoke-virtual {v0}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;
    move-result-object v0
    invoke-virtual {p0, v0}, Lacceptance/RecipeObserver;->editor_shell(Ljava/lang/String;)Ljava/lang/String;
    const-string v0, "logcat -d -v raw -s GGRecipeIme:I"
    invoke-virtual {p0, v0}, Lacceptance/RecipeObserver;->editor_shell(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    const-string v1, "PASS phase="
    invoke-virtual {v1, p1}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v1
    invoke-virtual {v0, v1}, Ljava/lang/String;->contains(Ljava/lang/CharSequence;)Z
    move-result v0
    if-nez v0, :done
    new-instance v0, Ljava/lang/IllegalStateException;
    const-string v1, "IME receipt missing: "
    invoke-virtual {v1, p1}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v1
    invoke-direct {v0, v1}, Ljava/lang/IllegalStateException;-><init>(Ljava/lang/String;)V
    throw v0
  :done
    new-instance v0, Landroid/os/Bundle;
    invoke-direct {v0}, Landroid/os/Bundle;-><init>()V
    const-string v1, "system_ime_phase"
    invoke-virtual {v0, v1, p1}, Landroid/os/Bundle;->putString(Ljava/lang/String;Ljava/lang/String;)V
    const/4 v1, 1
    invoke-virtual {p0, v1, v0}, Landroid/app/Instrumentation;->sendStatus(ILandroid/os/Bundle;)V
    return-void
.end method

.method private write(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    .registers 11
    invoke-direct {p0}, Lacceptance/RecipeEditorObserver;->keyboard()V
    move-object v0, p0
    move-object v1, p1
    move-object v2, p2
    move-object v3, p3
    move-object v4, p3
    const/4 v5, 0
    const/4 v6, 0
    invoke-direct/range {v0 .. v6}, Lacceptance/RecipeEditorObserver;->ime(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V
    return-void
.end method

.method private block_save()V
    .registers 7
    invoke-virtual {p0}, Landroid/app/Instrumentation;->getTargetContext()Landroid/content/Context;
    move-result-object v0
    invoke-virtual {v0}, Landroid/content/Context;->getFilesDir()Ljava/io/File;
    move-result-object v0
    new-instance v1, Ljava/io/File;
    const-string v2, "recipes"
    invoke-direct {v1, v0, v2}, Ljava/io/File;-><init>(Ljava/io/File;Ljava/lang/String;)V
    invoke-virtual {v1}, Ljava/io/File;->listFiles()[Ljava/io/File;
    move-result-object v0
    array-length v1, v0
    const/4 v2, 1
    if-ne v1, v2, :fail
    const/4 v1, 0
    aget-object v0, v0, v1
    invoke-virtual {v0}, Ljava/io/File;->getName()Ljava/lang/String;
    move-result-object v1
    const-string v2, ".recipe"
    invoke-virtual {v1, v2}, Ljava/lang/String;->endsWith(Ljava/lang/String;)Z
    move-result v1
    if-eqz v1, :fail
    invoke-virtual {v0}, Ljava/io/File;->getPath()Ljava/lang/String;
    move-result-object v1
    const-string v2, ".held"
    invoke-virtual {v1, v2}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v1
    new-instance v2, Ljava/io/File;
    invoke-direct {v2, v1}, Ljava/io/File;-><init>(Ljava/lang/String;)V
    invoke-virtual {v0, v2}, Ljava/io/File;->renameTo(Ljava/io/File;)Z
    move-result v1
    if-eqz v1, :fail
    iput-object v0, p0, Lacceptance/RecipeEditorObserver;->blocked:Ljava/io/File;
    iput-object v2, p0, Lacceptance/RecipeEditorObserver;->held:Ljava/io/File;
    invoke-virtual {v0}, Ljava/io/File;->mkdir()Z
    move-result v1
    if-nez v1, :done
  :fail
    new-instance v0, Ljava/lang/IllegalStateException;
    const-string v1, "Could not construct exact save-failure fixture"
    invoke-direct {v0, v1}, Ljava/lang/IllegalStateException;-><init>(Ljava/lang/String;)V
    throw v0
  :done
    return-void
.end method

.method private unblock_save()V
    .registers 4
    iget-object v0, p0, Lacceptance/RecipeEditorObserver;->blocked:Ljava/io/File;
    if-eqz v0, :done
    invoke-virtual {v0}, Ljava/io/File;->delete()Z
    move-result v1
    if-eqz v1, :fail
    iget-object v1, p0, Lacceptance/RecipeEditorObserver;->held:Ljava/io/File;
    invoke-virtual {v1, v0}, Ljava/io/File;->renameTo(Ljava/io/File;)Z
    move-result v1
    if-eqz v1, :fail
    const/4 v0, 0
    iput-object v0, p0, Lacceptance/RecipeEditorObserver;->blocked:Ljava/io/File;
    iput-object v0, p0, Lacceptance/RecipeEditorObserver;->held:Ljava/io/File;
  :done
    return-void
  :fail
    new-instance v0, Ljava/lang/IllegalStateException;
    const-string v1, "Could not restore save-failure fixture"
    invoke-direct {v0, v1}, Ljava/lang/IllegalStateException;-><init>(Ljava/lang/String;)V
    throw v0
.end method

.method public onStart()V
    .registers 12
  :start
    invoke-virtual {p0}, Lacceptance/RecipeObserver;->editor_launch()V
    const-string v0, "stores-home"
    invoke-virtual {p0, v0}, Lacceptance/RecipeObserver;->editor_expect(Ljava/lang/String;)V
    const/4 v0, 2
    const/4 v1, 1
    invoke-virtual {p0, v0, v1}, Lacceptance/RecipeObserver;->editor_tap(II)V
    const-string v0, "edit-empty"
    invoke-virtual {p0, v0}, Lacceptance/RecipeObserver;->editor_expect(Ljava/lang/String;)V
    const/4 v0, 0
    const/4 v1, 1
    const-string v2, "edit-new"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const/4 v1, 0
    const-string v2, "edit-invalid"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const/4 v0, 1
    const-string v2, "edit-name"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const-string v0, "name-compose-one"
    const-string v1, "compose"
    const-string v2, "tug"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->write(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const-string v0, "name-compose-two"
    const-string v2, "tugba"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->write(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const-string v0, "name-commit"
    const-string v1, "commit"
    const-string v2, "name"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->write(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    # Select an entire supplementary character through the real remote IC.
    move-object v0, p0
    const-string v1, "name-select-owl"
    const-string v2, "select"
    const-string v3, "empty"
    const-string v4, "name"
    const/4 v5, 7
    const/16 v6, 9
    invoke-direct/range {v0 .. v6}, Lacceptance/RecipeEditorObserver;->ime(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V
    const-string v1, "name-replace-owl"
    const-string v2, "commit"
    const-string v3, "food"
    const-string v4, "name-food"
    const/4 v5, 0
    const/4 v6, 0
    invoke-direct/range {v0 .. v6}, Lacceptance/RecipeEditorObserver;->ime(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V
    const-string v1, "name-delete-point"
    const-string v2, "delete-points"
    const-string v3, "empty"
    const-string v4, "name-prefix"
    const/4 v5, 1
    invoke-direct/range {v0 .. v6}, Lacceptance/RecipeEditorObserver;->ime(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V
    const-string v1, "name-restore-owl"
    const-string v2, "commit"
    const-string v3, "owl"
    const-string v4, "name"
    const/4 v5, 0
    invoke-direct/range {v0 .. v6}, Lacceptance/RecipeEditorObserver;->ime(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V
    const-string v1, "name-sdk-replace-food"
    const-string v2, "replace"
    const-string v3, "food"
    const-string v4, "name-food"
    const/4 v5, 7
    const/16 v6, 9
    invoke-direct/range {v0 .. v6}, Lacceptance/RecipeEditorObserver;->ime(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V
    const-string v1, "name-sdk-replace-owl"
    const-string v3, "owl"
    const-string v4, "name"
    const/16 v6, 8
    invoke-direct/range {v0 .. v6}, Lacceptance/RecipeEditorObserver;->ime(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V
    const-string v1, "name-select-copy"
    const-string v2, "select"
    const-string v3, "empty"
    const/4 v5, 0
    const/4 v6, 5
    invoke-direct/range {v0 .. v6}, Lacceptance/RecipeEditorObserver;->ime(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V
    const-string v1, "name-copy"
    const-string v2, "context"
    const v5, 16908321
    const/4 v6, 0
    invoke-direct/range {v0 .. v6}, Lacceptance/RecipeEditorObserver;->ime(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V
    const-string v1, "name-end"
    const-string v2, "select"
    const/16 v5, 9
    const/16 v6, 9
    invoke-direct/range {v0 .. v6}, Lacceptance/RecipeEditorObserver;->ime(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V
    const-string v1, "name-space"
    const-string v2, "commit"
    const-string v3, "space"
    const-string v4, "name-space"
    const/4 v5, 0
    const/4 v6, 0
    invoke-direct/range {v0 .. v6}, Lacceptance/RecipeEditorObserver;->ime(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V
    const-string v1, "name-paste"
    const-string v2, "context"
    const-string v3, "empty"
    const-string v4, "name-pasted"
    const v5, 16908322
    invoke-direct/range {v0 .. v6}, Lacceptance/RecipeEditorObserver;->ime(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V
    const-string v1, "name-select-cut"
    const-string v2, "select"
    const/16 v5, 9
    const/16 v6, 15
    invoke-direct/range {v0 .. v6}, Lacceptance/RecipeEditorObserver;->ime(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V
    const-string v1, "name-cut"
    const-string v2, "context"
    const-string v4, "name"
    const v5, 16908320
    const/4 v6, 0
    invoke-direct/range {v0 .. v6}, Lacceptance/RecipeEditorObserver;->ime(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V
    const-string v1, "remember-name"
    const-string v2, "remember"
    const/4 v5, 0
    invoke-direct/range {v0 .. v6}, Lacceptance/RecipeEditorObserver;->ime(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V
    const/4 v0, 0
    const/4 v1, 0
    const-string v2, "edit-named"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const/4 v0, 2
    const-string v2, "edit-servings"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    invoke-direct {p0}, Lacceptance/RecipeEditorObserver;->keyboard()V
    move-object v0, p0
    const-string v1, "stale-name-refused"
    const-string v2, "stale"
    const-string v3, "wrong"
    const-string v4, "one"
    const/4 v5, 0
    const/4 v6, 0
    invoke-direct/range {v0 .. v6}, Lacceptance/RecipeEditorObserver;->ime(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V
    const-string v0, "invalid-servings"
    const-string v1, "all"
    const-string v2, "zero"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->write(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const/4 v0, 0
    const/4 v1, 0
    const-string v2, "edit-zero"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const-string v2, "edit-invalid-servings"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const/4 v0, 2
    const-string v2, "edit-servings-retry"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const-string v0, "valid-servings"
    const-string v1, "all"
    const-string v2, "two"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->write(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const/4 v0, 0
    const/4 v1, 0
    const-string v2, "edit-portions"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const/4 v0, 3
    const-string v2, "edit-ingredient"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const/4 v0, 1
    const-string v2, "edit-ingredient-name"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const-string v0, "ingredient-name"
    const-string v1, "commit"
    const-string v2, "rice"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->write(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const/4 v0, 0
    const/4 v1, 0
    const-string v2, "edit-ingredient-named"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const/4 v0, 2
    const-string v2, "edit-quantity"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const-string v0, "fractional-quantity"
    const-string v1, "commit"
    const-string v2, "fraction"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->write(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const/4 v0, 0
    const/4 v1, 0
    const-string v2, "edit-measured"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const-string v2, "edit-overview"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const/4 v0, 3
    const/4 v1, 1
    const-string v2, "edit-source"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const/4 v0, 1
    const/4 v1, 0
    const-string v2, "edit-link"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const-string v0, "source-link"
    const-string v1, "commit"
    const-string v2, "link"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->write(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const/4 v0, 0
    const/4 v1, 0
    const-string v2, "edit-linked"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const/4 v0, 2
    const-string v2, "edit-note"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const-string v0, "unicode-nul-source-note"
    const-string v1, "commit"
    const-string v2, "note"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->write(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const/4 v0, 0
    const/4 v1, 0
    const-string v2, "edit-noted"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const-string v2, "edit-complete"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const/4 v1, 1
    const-string v2, "edit-cancel"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const/4 v1, 0
    const-string v2, "edit-kept"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const-string v2, "edit-saved"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const/4 v1, 2
    const-string v2, "edit-existing"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const/4 v0, 4
    const/4 v1, 0
    const-string v2, "edit-existing-ingredient"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const/4 v0, 2
    const-string v2, "edit-existing-quantity"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const-string v0, "decimal-edit"
    const-string v1, "all"
    const-string v2, "decimal"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->write(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const/4 v0, 0
    const/4 v1, 0
    const-string v2, "edit-changed"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const-string v2, "edit-changed-overview"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    invoke-direct {p0}, Lacceptance/RecipeEditorObserver;->block_save()V
    const-string v2, "edit-save-failed"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    invoke-direct {p0}, Lacceptance/RecipeEditorObserver;->unblock_save()V
    const-string v2, "edit-updated"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const/4 v1, 1
    const-string v2, "edit-recipes"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const/4 v0, 1
    const/4 v1, 0
    const-string v2, "edit-selected"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const/4 v0, 2
    const/4 v1, 1
    const-string v2, "edit-plan"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    const/4 v0, 0
    const-string v2, "edit-scaled"
    invoke-direct {p0, v0, v1, v2}, Lacceptance/RecipeEditorObserver;->tap(IILjava/lang/String;)V
    new-instance v0, Landroid/os/Bundle;
    invoke-direct {v0}, Landroid/os/Bundle;-><init>()V
    const-string v1, "stream"
    const-string v2, "PASS installed recipe creation, system IME composition, Unicode/NUL, selection, clipboard, validation, failed save, edit and exact planning\n"
    invoke-virtual {v0, v1, v2}, Landroid/os/Bundle;->putString(Ljava/lang/String;Ljava/lang/String;)V
    const/4 v1, -1
    invoke-virtual {p0, v1, v0}, Landroid/app/Instrumentation;->finish(ILandroid/os/Bundle;)V
  :end
    return-void
    .catch Ljava/lang/Throwable; {:start .. :end} :failed
  :failed
    move-exception v0
    const-string v1, "GGRecipeEditorObserver"
    const-string v2, "Installed recipe editor observation failed"
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
