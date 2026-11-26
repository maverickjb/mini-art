.class public final enum Lminiart/staticinit/TileMode;
.super Ljava/lang/Enum;

.field public static final enum CLAMP:Lminiart/staticinit/TileMode;

.field public static final enum REPEAT:Lminiart/staticinit/TileMode;

.field public static final enum MIRROR:Lminiart/staticinit/TileMode;

.field private static final synthetic $VALUES:[Lminiart/staticinit/TileMode;

.method static constructor <clinit>()V
    .locals 5

    new-instance v0, Lminiart/staticinit/TileMode;

    const-string v3, "CLAMP"

    const/4 v4, 0x0

    invoke-direct {v0, v3, v4}, Lminiart/staticinit/TileMode;-><init>(Ljava/lang/String;I)V

    sput-object v0, Lminiart/staticinit/TileMode;->CLAMP:Lminiart/staticinit/TileMode;

    new-instance v1, Lminiart/staticinit/TileMode;

    const-string v3, "REPEAT"

    const/4 v4, 0x1

    invoke-direct {v1, v3, v4}, Lminiart/staticinit/TileMode;-><init>(Ljava/lang/String;I)V

    sput-object v1, Lminiart/staticinit/TileMode;->REPEAT:Lminiart/staticinit/TileMode;

    new-instance v2, Lminiart/staticinit/TileMode;

    const-string v3, "MIRROR"

    const/4 v4, 0x2

    invoke-direct {v2, v3, v4}, Lminiart/staticinit/TileMode;-><init>(Ljava/lang/String;I)V

    sput-object v2, Lminiart/staticinit/TileMode;->MIRROR:Lminiart/staticinit/TileMode;

    const/4 v3, 0x3

    new-array v3, v3, [Lminiart/staticinit/TileMode;

    const/4 v4, 0x0

    aput-object v0, v3, v4

    const/4 v4, 0x1

    aput-object v1, v3, v4

    const/4 v4, 0x2

    aput-object v2, v3, v4

    sput-object v3, Lminiart/staticinit/TileMode;->$VALUES:[Lminiart/staticinit/TileMode;

    return-void
.end method

.method private constructor <init>(Ljava/lang/String;I)V
    .locals 0

    invoke-direct {p0, p1, p2}, Ljava/lang/Enum;-><init>(Ljava/lang/String;I)V

    return-void
.end method
