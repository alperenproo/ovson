package net.ovson.api.net;
import java.util.Map;

public class HttpResponse {
    public final int statusCode;
    public final String body;
    public final Map<String, String> headers;
    
    public HttpResponse(int statusCode, String body, Map<String, String> headers) {
        this.statusCode = statusCode;
        this.body = body;
        this.headers = headers;
    }
    
    public boolean isSuccess() {
        return statusCode >= 200 && statusCode < 300;
    }
}
