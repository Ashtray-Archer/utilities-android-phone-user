.class public Lacceptance/RecipeImeCommand;
.super Landroid/content/BroadcastReceiver;

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Landroid/content/BroadcastReceiver;-><init>()V
    return-void
.end method

.method private static asset(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;
    .registers 3
    invoke-virtual {p0}, Landroid/content/Context;->getAssets()Landroid/content/res/AssetManager;
    move-result-object v0
    invoke-virtual {v0, p1}, Landroid/content/res/AssetManager;->open(Ljava/lang/String;)Ljava/io/InputStream;
    move-result-object v0
    invoke-static {v0}, Lacceptance/RecipeObserver;->read(Ljava/io/InputStream;)Ljava/lang/String;
    move-result-object v0
    return-object v0
.end method

.method public onReceive(Landroid/content/Context;Landroid/content/Intent;)V
    .registers 15
    const-string v0, "phase"
    invoke-virtual {p2, v0}, Landroid/content/Intent;->getStringExtra(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
  :start
    const-string v1, "operation"
    invoke-virtual {p2, v1}, Landroid/content/Intent;->getStringExtra(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v1
    const-string v2, "text"
    invoke-virtual {p2, v2}, Landroid/content/Intent;->getStringExtra(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v2
    if-eqz v2, :no_text
    invoke-static {p1, v2}, Lacceptance/RecipeImeCommand;->asset(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;
    move-result-object v2
    goto :arguments
  :no_text
    const-string v2, ""
  :arguments
    const-string v3, "first"
    const/4 v4, 0
    invoke-virtual {p2, v3, v4}, Landroid/content/Intent;->getIntExtra(Ljava/lang/String;I)I
    move-result v3
    const-string v5, "last"
    invoke-virtual {p2, v5, v4}, Landroid/content/Intent;->getIntExtra(Ljava/lang/String;I)I
    move-result v5
    sget-object v6, Lacceptance/RecipeTestIme;->current:Lacceptance/RecipeTestIme;
    invoke-virtual {v6}, Landroid/inputmethodservice/InputMethodService;->getCurrentInputConnection()Landroid/view/inputmethod/InputConnection;
    move-result-object v6
    if-nez v6, :operation
    new-instance v7, Ljava/lang/IllegalStateException;
    const-string v8, "No system input connection"
    invoke-direct {v7, v8}, Ljava/lang/IllegalStateException;-><init>(Ljava/lang/String;)V
    throw v7
  :operation
    const-string v7, "commit"
    invoke-virtual {v7, v1}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v7
    if-eqz v7, :compose
    const/4 v7, 1
    invoke-interface {v6, v2, v7}, Landroid/view/inputmethod/InputConnection;->commitText(Ljava/lang/CharSequence;I)Z
    goto :observe
  :compose
    const-string v7, "compose"
    invoke-virtual {v7, v1}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v7
    if-eqz v7, :selection
    const/4 v7, 1
    invoke-interface {v6, v2, v7}, Landroid/view/inputmethod/InputConnection;->setComposingText(Ljava/lang/CharSequence;I)Z
    goto :observe
  :selection
    const-string v7, "select"
    invoke-virtual {v7, v1}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v7
    if-eqz v7, :delete_points
    invoke-interface {v6, v3, v5}, Landroid/view/inputmethod/InputConnection;->setSelection(II)Z
    goto :observe
  :delete_points
    const-string v7, "delete-points"
    invoke-virtual {v7, v1}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v7
    if-eqz v7, :delete_units
    invoke-interface {v6, v3, v5}, Landroid/view/inputmethod/InputConnection;->deleteSurroundingTextInCodePoints(II)Z
    goto :observe
  :delete_units
    const-string v7, "delete-units"
    invoke-virtual {v7, v1}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v7
    if-eqz v7, :finish
    invoke-interface {v6, v3, v5}, Landroid/view/inputmethod/InputConnection;->deleteSurroundingText(II)Z
    goto :observe
  :finish
    const-string v7, "finish"
    invoke-virtual {v7, v1}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v7
    if-eqz v7, :context
    invoke-interface {v6}, Landroid/view/inputmethod/InputConnection;->finishComposingText()Z
    goto :observe
  :context
    const-string v7, "context"
    invoke-virtual {v7, v1}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v7
    if-eqz v7, :remember
    invoke-interface {v6, v3}, Landroid/view/inputmethod/InputConnection;->performContextMenuAction(I)Z
    goto :observe
  :remember
    const-string v7, "remember"
    invoke-virtual {v7, v1}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v7
    if-eqz v7, :stale
    sput-object v6, Lacceptance/RecipeTestIme;->remembered:Landroid/view/inputmethod/InputConnection;
    goto :observe
  :stale
    const-string v7, "stale"
    invoke-virtual {v7, v1}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v7
    if-eqz v7, :all
    sget-object v7, Lacceptance/RecipeTestIme;->remembered:Landroid/view/inputmethod/InputConnection;
    const/4 v8, 1
    invoke-interface {v7, v2, v8}, Landroid/view/inputmethod/InputConnection;->commitText(Ljava/lang/CharSequence;I)Z
    goto :observe
  :all
    const-string v7, "all"
    invoke-virtual {v7, v1}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v7
    if-eqz v7, :replace
    const v7, 16908319
    invoke-interface {v6, v7}, Landroid/view/inputmethod/InputConnection;->performContextMenuAction(I)Z
    const/4 v7, 1
    invoke-interface {v6, v2, v7}, Landroid/view/inputmethod/InputConnection;->commitText(Ljava/lang/CharSequence;I)Z
    goto :observe
  :replace
    const-string v7, "replace"
    invoke-virtual {v7, v1}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v7
    if-eqz v7, :unknown
    sget v7, Landroid/os/Build$VERSION;->SDK_INT:I
    const/16 v8, 34
    if-lt v7, v8, :legacy_replace
    invoke-static {v6, v3, v5, v2}, Lacceptance/RecipeImeModern;->replace(Landroid/view/inputmethod/InputConnection;IILjava/lang/CharSequence;)V
    goto :observe
  :legacy_replace
    invoke-interface {v6}, Landroid/view/inputmethod/InputConnection;->finishComposingText()Z
    invoke-interface {v6, v3, v5}, Landroid/view/inputmethod/InputConnection;->setSelection(II)Z
    const/4 v7, 1
    invoke-interface {v6, v2, v7}, Landroid/view/inputmethod/InputConnection;->commitText(Ljava/lang/CharSequence;I)Z
    goto :observe
  :unknown
    new-instance v7, Ljava/lang/IllegalArgumentException;
    invoke-direct {v7, v1}, Ljava/lang/IllegalArgumentException;-><init>(Ljava/lang/String;)V
    throw v7
  :observe
    # Synchronous SDK reads follow the mutating binder messages. Observe the
    # real served editor, including exact Unicode/NUL, without app exports.
    const v7, 262144
    invoke-interface {v6, v7, v4}, Landroid/view/inputmethod/InputConnection;->getTextBeforeCursor(II)Ljava/lang/CharSequence;
    move-result-object v7
    invoke-interface {v6, v4}, Landroid/view/inputmethod/InputConnection;->getSelectedText(I)Ljava/lang/CharSequence;
    move-result-object v8
    const v9, 262144
    invoke-interface {v6, v9, v4}, Landroid/view/inputmethod/InputConnection;->getTextAfterCursor(II)Ljava/lang/CharSequence;
    move-result-object v9
    if-eqz v7, :missing
    if-nez v9, :build
  :missing
    new-instance v10, Ljava/lang/IllegalStateException;
    const-string v11, "SDK text query returned null"
    invoke-direct {v10, v11}, Ljava/lang/IllegalStateException;-><init>(Ljava/lang/String;)V
    throw v10
  :build
    new-instance v10, Ljava/lang/StringBuilder;
    invoke-direct {v10}, Ljava/lang/StringBuilder;-><init>()V
    invoke-virtual {v10, v7}, Ljava/lang/StringBuilder;->append(Ljava/lang/CharSequence;)Ljava/lang/StringBuilder;
    if-eqz v8, :after
    invoke-virtual {v10, v8}, Ljava/lang/StringBuilder;->append(Ljava/lang/CharSequence;)Ljava/lang/StringBuilder;
  :after
    invoke-virtual {v10, v9}, Ljava/lang/StringBuilder;->append(Ljava/lang/CharSequence;)Ljava/lang/StringBuilder;
    invoke-virtual {v10}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;
    move-result-object v10
    const-string v11, "expected"
    invoke-virtual {p2, v11}, Landroid/content/Intent;->getStringExtra(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v11
    invoke-static {p1, v11}, Lacceptance/RecipeImeCommand;->asset(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;
    move-result-object v11
    invoke-virtual {v11, v10}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v11
    if-nez v11, :pass
    new-instance v11, Ljava/lang/IllegalStateException;
    invoke-direct {v11, v10}, Ljava/lang/IllegalStateException;-><init>(Ljava/lang/String;)V
    throw v11
  :pass
    const-string v1, "PASS phase="
    invoke-virtual {v1, v0}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v1
    const-string v2, "GGRecipeIme"
    invoke-static {v2, v1}, Landroid/util/Log;->i(Ljava/lang/String;Ljava/lang/String;)I
    return-void
  :end
    .catch Ljava/lang/Throwable; {:start .. :end} :failed
  :failed
    move-exception v1
    const-string v2, "FAIL phase="
    invoke-virtual {v2, v0}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    const-string v2, "GGRecipeIme"
    invoke-static {v2, v0, v1}, Landroid/util/Log;->e(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I
    return-void
.end method
