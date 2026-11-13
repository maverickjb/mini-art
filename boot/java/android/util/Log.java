package android.util;

public final class Log {
    public static final int VERBOSE = 2;
    public static final int DEBUG = 3;
    public static final int INFO = 4;
    public static final int WARN = 5;
    public static final int ERROR = 6;

    private Log() {
    }

    public static int i(String tag, String msg) {
        return println(INFO, tag, msg);
    }

    public static native int println(int priority, String tag, String msg);
}
