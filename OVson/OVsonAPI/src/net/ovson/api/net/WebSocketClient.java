package net.ovson.api.net;

public abstract class WebSocketClient {
    protected String url;
    protected boolean connected;
    
    public WebSocketClient(String url) {
        this.url = url;
    }
    
    public abstract void onOpen();
    public abstract void onMessage(String message);
    public abstract void onClose();
    public abstract void onError(Exception e);
    
    public void connect() {
        // Basic stub for standard extension
    }
    
    public void send(String message) {
        // Basic stub
    }
    
    public void close() {
        // Basic stub
    }
    
    public boolean isConnected() {
        return connected;
    }
}
