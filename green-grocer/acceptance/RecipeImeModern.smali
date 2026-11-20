.class public Lacceptance/RecipeImeModern;
.super Ljava/lang/Object;

# Only loaded by the API34 branch. This is an SDK call in the test keyboard,
# not an application callback producer or semantic implementation.
.method public static replace(Landroid/view/inputmethod/InputConnection;IILjava/lang/CharSequence;)V
    .registers 6
    move-object v0, p0
    move v1, p1
    move v2, p2
    move-object v3, p3
    const/4 v4, 1
    const/4 v5, 0
    invoke-interface/range {v0 .. v5}, Landroid/view/inputmethod/InputConnection;->replaceText(IILjava/lang/CharSequence;ILandroid/view/inputmethod/TextAttribute;)Z
    return-void
.end method
