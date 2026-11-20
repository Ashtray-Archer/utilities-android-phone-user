.class public Lacceptance/RecipeTestIme;
.super Landroid/inputmethodservice/InputMethodService;
.field public static current:Lacceptance/RecipeTestIme;
.field public static remembered:Landroid/view/inputmethod/InputConnection;

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Landroid/inputmethodservice/InputMethodService;-><init>()V
    return-void
.end method

.method public onCreate()V
    .registers 1
    invoke-super {p0}, Landroid/inputmethodservice/InputMethodService;->onCreate()V
    sput-object p0, Lacceptance/RecipeTestIme;->current:Lacceptance/RecipeTestIme;
    return-void
.end method

.method public onCreateInputView()Landroid/view/View;
    .registers 3
    new-instance v0, Landroid/widget/TextView;
    invoke-direct {v0, p0}, Landroid/widget/TextView;-><init>(Landroid/content/Context;)V
    const-string v1, "Green Grocer acceptance keyboard"
    invoke-virtual {v0, v1}, Landroid/widget/TextView;->setText(Ljava/lang/CharSequence;)V
    const/16 v1, 300
    invoke-virtual {v0, v1}, Landroid/view/View;->setMinimumHeight(I)V
    return-object v0
.end method

.method public onStartInputView(Landroid/view/inputmethod/EditorInfo;Z)V
    .registers 5
    invoke-super {p0, p1, p2}, Landroid/inputmethodservice/InputMethodService;->onStartInputView(Landroid/view/inputmethod/EditorInfo;Z)V
    const-string v0, "GGRecipeIme"
    const-string v1, "SHOWN system-bound recipe input connection"
    invoke-static {v0, v1}, Landroid/util/Log;->i(Ljava/lang/String;Ljava/lang/String;)I
    return-void
.end method
