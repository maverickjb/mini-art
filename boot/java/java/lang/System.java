package java.lang;

public final class System {
    private System() {
    }

    public static native void arraycopy(Object src, int srcPos, Object dst, int dstPos, int length);
}
