.class public final Lacceptance/RecipeDeliver;
.super Ljava/lang/Object;
.implements Ljava/lang/Runnable;
.field private activity:Landroid/app/Activity;
.field private intent:Landroid/content/Intent;

.method public constructor <init>(Landroid/app/Activity;Landroid/content/Intent;)V
    .registers 3
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    iput-object p1, p0, Lacceptance/RecipeDeliver;->activity:Landroid/app/Activity;
    iput-object p2, p0, Lacceptance/RecipeDeliver;->intent:Landroid/content/Intent;
    return-void
.end method

.method public run()V
    .registers 5
    iget-object v0, p0, Lacceptance/RecipeDeliver;->activity:Landroid/app/Activity;
    check-cast v0, Lorg/isomorphisms/greengrocer/RecipeActivity;
    iget-object v3, p0, Lacceptance/RecipeDeliver;->intent:Landroid/content/Intent;
    const/16 v1, 412
    const/4 v2, -1
    invoke-virtual {v0, v1, v2, v3}, Lorg/isomorphisms/greengrocer/RecipeActivity;->onActivityResult(IILandroid/content/Intent;)V
    return-void
.end method
