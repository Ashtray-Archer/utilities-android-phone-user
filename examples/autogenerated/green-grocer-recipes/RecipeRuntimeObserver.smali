.class public final Lacceptance/RecipeRuntimeObserver;
.super Ljava/lang/Object;

# Independent caller only. Recipe operations execute in source-generated DEX.
.method private static trace(Ljava/lang/String;)V
    .registers 2
    sget-object v0, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v0, p0}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
    return-void
.end method

.method private static require(ZLjava/lang/String;)V
    .registers 3
    if-nez p0, :passed
    sget-object v0, Ljava/lang/System;->err:Ljava/io/PrintStream;
    invoke-virtual {v0, p1}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
    const/16 v0, 0x49
    invoke-static {v0}, Ljava/lang/System;->exit(I)V
  :passed
    return-void
.end method

.method private static integer(IILjava/lang/String;)V
    .registers 4
    const/4 v0, 0
    if-ne p0, p1, :compared
    const/4 v0, 1
  :compared
    invoke-static {v0, p2}, Lacceptance/RecipeRuntimeObserver;->require(ZLjava/lang/String;)V
    return-void
.end method

.method private static text(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    .registers 4
    invoke-virtual {p0, p1}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v0
    invoke-static {v0, p2}, Lacceptance/RecipeRuntimeObserver;->require(ZLjava/lang/String;)V
    return-void
.end method

.method public static main([Ljava/lang/String;)V
    .registers 5
    sget-object v0, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v1, "observer_pid\t"
    invoke-virtual {v0, v1}, Ljava/io/PrintStream;->print(Ljava/lang/String;)V
    invoke-static {}, Landroid/os/Process;->myPid()I
    move-result v1
    invoke-virtual {v0, v1}, Ljava/io/PrintStream;->println(I)V
    const-string v3, "CALL packages_for_grams -1"
    invoke-static {v3}, Lacceptance/RecipeRuntimeObserver;->trace(Ljava/lang/String;)V
    const/4 v0, -0x1
    invoke-static {v0}, LIdric/Generated;->packages_for_grams(I)I
    move-result v1
    const/4 v2, -0x1
    const-string v3, "FAIL negative demand refusal"
    invoke-static {v1, v2, v3}, Lacceptance/RecipeRuntimeObserver;->integer(IILjava/lang/String;)V
    const-string v3, "CALL packages_for_grams 0"
    invoke-static {v3}, Lacceptance/RecipeRuntimeObserver;->trace(Ljava/lang/String;)V
    const/4 v0, 0
    invoke-static {v0}, LIdric/Generated;->packages_for_grams(I)I
    move-result v1
    const/4 v2, 0
    const-string v3, "FAIL zero demand"
    invoke-static {v1, v2, v3}, Lacceptance/RecipeRuntimeObserver;->integer(IILjava/lang/String;)V
    const-string v3, "CALL packages_for_grams 300"
    invoke-static {v3}, Lacceptance/RecipeRuntimeObserver;->trace(Ljava/lang/String;)V
    const/16 v0, 300
    invoke-static {v0}, LIdric/Generated;->packages_for_grams(I)I
    move-result v1
    const/4 v2, 1
    const-string v3, "FAIL exact package boundary"
    invoke-static {v1, v2, v3}, Lacceptance/RecipeRuntimeObserver;->integer(IILjava/lang/String;)V
    const-string v3, "CALL packages_for_grams 301"
    invoke-static {v3}, Lacceptance/RecipeRuntimeObserver;->trace(Ljava/lang/String;)V
    const/16 v0, 301
    invoke-static {v0}, LIdric/Generated;->packages_for_grams(I)I
    move-result v1
    const/4 v2, 2
    const-string v3, "FAIL package boundary plus one"
    invoke-static {v1, v2, v3}, Lacceptance/RecipeRuntimeObserver;->integer(IILjava/lang/String;)V
    const-string v3, "CALL packages_for_grams 500"
    invoke-static {v3}, Lacceptance/RecipeRuntimeObserver;->trace(Ljava/lang/String;)V
    const/16 v0, 500
    invoke-static {v0}, LIdric/Generated;->packages_for_grams(I)I
    move-result v1
    const-string v3, "FAIL ordinary package rounding"
    invoke-static {v1, v2, v3}, Lacceptance/RecipeRuntimeObserver;->integer(IILjava/lang/String;)V
    const-string v3, "CALL packages_for_grams 2147483647"
    invoke-static {v3}, Lacceptance/RecipeRuntimeObserver;->trace(Ljava/lang/String;)V
    const v0, 2147483647
    invoke-static {v0}, LIdric/Generated;->packages_for_grams(I)I
    move-result v1
    const v2, 7158279
    const-string v3, "FAIL exact arithmetic at Int32 input limit"
    invoke-static {v1, v2, v3}, Lacceptance/RecipeRuntimeObserver;->integer(IILjava/lang/String;)V
    const-string v3, "CALL saved_recipe_name Unicode"
    invoke-static {v3}, Lacceptance/RecipeRuntimeObserver;->trace(Ljava/lang/String;)V
    const-string v0, "Soup λ🦉"
    invoke-static {v0}, LIdric/Generated;->saved_recipe_name(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v1
    const-string v3, "FAIL saved Unicode recipe name"
    invoke-static {v1, v0, v3}, Lacceptance/RecipeRuntimeObserver;->text(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const-string v0, ""
    invoke-static {v0}, LIdric/Generated;->saved_recipe_name(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v1
    const-string v2, "Recipe needs an identity and name"
    const-string v3, "FAIL empty recipe name refusal"
    invoke-static {v1, v2, v3}, Lacceptance/RecipeRuntimeObserver;->text(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const-string v3, "CALL saved_recipe_source_link"
    invoke-static {v3}, Lacceptance/RecipeRuntimeObserver;->trace(Ljava/lang/String;)V
    const-string v0, "https://www.youtube.com/watch?v=fixture-video#t=80"
    invoke-static {v0}, LIdric/Generated;->saved_recipe_source_link(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v1
    const-string v3, "FAIL exact video source preservation"
    invoke-static {v1, v0, v3}, Lacceptance/RecipeRuntimeObserver;->text(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const-string v0, "https://fixture.invalid/食事/🦉?a=1&b=2"
    invoke-static {v0}, LIdric/Generated;->saved_recipe_source_link(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v1
    const-string v3, "FAIL Unicode source preservation"
    invoke-static {v1, v0, v3}, Lacceptance/RecipeRuntimeObserver;->text(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const-string v0, ""
    invoke-static {v0}, LIdric/Generated;->saved_recipe_source_link(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v1
    const-string v2, "Recipe source needs a link"
    const-string v3, "FAIL empty source refusal"
    invoke-static {v1, v2, v3}, Lacceptance/RecipeRuntimeObserver;->text(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const-string v3, "CALL ingredient_store_name"
    invoke-static {v3}, Lacceptance/RecipeRuntimeObserver;->trace(Ljava/lang/String;)V
    const-string v0, "000042"
    invoke-static {v0}, LIdric/Generated;->ingredient_store_name(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v1
    const-string v3, "FAIL leading-zero location preservation"
    invoke-static {v1, v0, v3}, Lacceptance/RecipeRuntimeObserver;->text(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const-string v0, "different-store"
    invoke-static {v0}, LIdric/Generated;->ingredient_store_name(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v1
    const-string v3, "FAIL changing store input"
    invoke-static {v1, v0, v3}, Lacceptance/RecipeRuntimeObserver;->text(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const-string v0, ""
    invoke-static {v0}, LIdric/Generated;->ingredient_store_name(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v1
    const-string v2, "default"
    const-string v3, "FAIL invalid override retains default store"
    invoke-static {v1, v2, v3}, Lacceptance/RecipeRuntimeObserver;->text(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    sget-object v0, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v1, "PASS changing recipe runtime inputs on ART"
    invoke-virtual {v0, v1}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
    return-void
.end method
