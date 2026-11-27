package android.graphics;

public class Typeface {

    public static final int NORMAL = 0;
    public static final int BOLD = 1;
    public static final int ITALIC = 2;
    public static final int BOLD_ITALIC = 3;

    public static final Typeface DEFAULT = new Typeface(null, NORMAL);
    public static final Typeface DEFAULT_BOLD = new Typeface(null, BOLD);
    public static final Typeface SANS_SERIF = new Typeface("sans-serif", NORMAL);
    public static final Typeface SERIF = new Typeface("serif", NORMAL);
    public static final Typeface MONOSPACE = new Typeface("monospace", NORMAL);

    private final String family;
    private final int style;

    private Typeface(String family, int style) {
        this.family = family;
        this.style = style;
    }
}
