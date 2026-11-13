package java.lang;

public class Object {
    public Object() {
    }

    public native int hashCode();

    public boolean equals(Object other) {
        return this == other;
    }
}
