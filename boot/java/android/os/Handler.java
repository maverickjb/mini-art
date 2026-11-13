package android.os;

public class Handler {
    private final Looper looper;

    public Handler() {
        this.looper = null;
    }

    public Handler(Looper looper) {
        this.looper = looper;
    }

    public Looper getLooper() {
        return looper;
    }
}
