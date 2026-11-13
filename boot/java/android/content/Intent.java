package android.content;

public class Intent {
    private final String action;

    public Intent() {
        this.action = null;
    }

    public Intent(String action) {
        this.action = action;
    }

    public String getAction() {
        return action;
    }
}
