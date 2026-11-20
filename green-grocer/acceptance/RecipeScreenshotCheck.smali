.class public Lacceptance/RecipeScreenshotCheck;
.super Ljava/lang/Object;

.method public static main([Ljava/lang/String;)V
    .registers 3
    const/4 v0, 0
    aget-object v0, p0, v0
    invoke-static {v0}, Landroid/graphics/BitmapFactory;->decodeFile(Ljava/lang/String;)Landroid/graphics/Bitmap;
    move-result-object v0
    if-eqz v0, :failed
    invoke-static {v0}, Lacceptance/RecipeObserver;->visible_canvas(Landroid/graphics/Bitmap;)Z
    move-result v1
    invoke-virtual {v0}, Landroid/graphics/Bitmap;->recycle()V
    if-eqz v1, :failed
    sget-object v0, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v1, "PASS native recipe background and button pixels visible"
    invoke-virtual {v0, v1}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
    return-void
  :failed
    const/4 v0, 1
    invoke-static {v0}, Ljava/lang/System;->exit(I)V
    return-void
.end method
