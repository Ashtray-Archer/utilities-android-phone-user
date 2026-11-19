.class public Lacceptance/RecipeViewHash;
.super Ljava/lang/Object;
.method public static main([Ljava/lang/String;)V
    .registers 4
    const/4 v0, 0
    aget-object v0, p0, v0
    new-instance v1, Ljava/io/FileInputStream;
    invoke-direct {v1, v0}, Ljava/io/FileInputStream;-><init>(Ljava/lang/String;)V
    invoke-static {v1}, Lacceptance/RecipeObserver;->read(Ljava/io/InputStream;)Ljava/lang/String;
    move-result-object v0
    invoke-static {v0}, Lacceptance/RecipeObserver;->hash(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v1, v0}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
    return-void
.end method
