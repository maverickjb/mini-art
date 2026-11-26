.class public Lminiart/staticinit/Main;
.super Landroid/app/Activity;

.method public constructor <init>()V
    .locals 0

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method

.method public onCreate(Landroid/os/Bundle;)V
    .locals 1

    invoke-super {p0}, Landroid/app/Activity;->onCreate(Landroid/os/Bundle;)V

    sget-object v0, Lminiart/staticinit/StaticObjectTest;->INSTANCE:Ljava/lang/Object;

    sget v0, Lminiart/staticinit/StaticValueTest;->value:I

    sget-object v0, Lminiart/staticinit/TileMode;->CLAMP:Lminiart/staticinit/TileMode;

    sget-object v0, Lminiart/staticinit/TileMode;->REPEAT:Lminiart/staticinit/TileMode;

    sget-object v0, Lminiart/staticinit/TileMode;->MIRROR:Lminiart/staticinit/TileMode;

    return-void
.end method
