package net.ovson.api.net;
import java.io.*;
import java.net.*;
import java.util.*;
import java.util.concurrent.CompletableFuture;

public final class HttpClient {
    private HttpClient() {}

    public static HttpResponse get(String urlStr) {
        return request("GET", urlStr, null, null);
    }
    
    public static HttpResponse get(String urlStr, Map<String, String> headers) {
        return request("GET", urlStr, headers, null);
    }
    
    public static HttpResponse post(String urlStr, String body, String contentType) {
        Map<String, String> headers = new HashMap<String, String>();
        headers.put("Content-Type", contentType);
        return request("POST", urlStr, headers, body);
    }
    
    public static HttpResponse post(String urlStr, String body, String contentType, Map<String, String> extraHeaders) {
        Map<String, String> headers = new HashMap<String, String>(extraHeaders);
        headers.put("Content-Type", contentType);
        return request("POST", urlStr, headers, body);
    }
    
    public static CompletableFuture<HttpResponse> getAsync(String urlStr) {
        return CompletableFuture.supplyAsync(() -> get(urlStr));
    }

    public static CompletableFuture<HttpResponse> getAsync(String urlStr, Map<String, String> headers) {
        return CompletableFuture.supplyAsync(() -> get(urlStr, headers));
    }
    
    public static CompletableFuture<HttpResponse> postAsync(String urlStr, String body, String contentType) {
        return CompletableFuture.supplyAsync(() -> post(urlStr, body, contentType));
    }

    public static CompletableFuture<HttpResponse> postAsync(String urlStr, String body, String contentType, Map<String, String> extraHeaders) {
        return CompletableFuture.supplyAsync(() -> post(urlStr, body, contentType, extraHeaders));
    }

    private static HttpResponse request(String method, String urlStr, Map<String, String> headers, String body) {
        try {
            URL url = new URL(urlStr);
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setRequestMethod(method);
            if (headers != null) {
                for (Map.Entry<String, String> entry : headers.entrySet()) {
                    conn.setRequestProperty(entry.getKey(), entry.getValue());
                }
            }
            if (body != null) {
                conn.setDoOutput(true);
                try (OutputStream os = conn.getOutputStream()) {
                    os.write(body.getBytes("UTF-8"));
                }
            }
            
            int code = conn.getResponseCode();
            InputStream is = code < 400 ? conn.getInputStream() : conn.getErrorStream();
            String responseBody = "";
            if (is != null) {
                try (BufferedReader br = new BufferedReader(new InputStreamReader(is, "UTF-8"))) {
                    StringBuilder sb = new StringBuilder();
                    String line;
                    while ((line = br.readLine()) != null) sb.append(line).append("\n");
                    responseBody = sb.toString();
                }
            }
            
            Map<String, String> respHeaders = new HashMap<String, String>();
            for (Map.Entry<String, List<String>> entry : conn.getHeaderFields().entrySet()) {
                if (entry.getKey() != null && !entry.getValue().isEmpty()) {
                    respHeaders.put(entry.getKey(), entry.getValue().get(0));
                }
            }
            
            return new HttpResponse(code, responseBody.trim(), respHeaders);
        } catch (Exception e) {
            return new HttpResponse(-1, e.getMessage(), new HashMap<String, String>());
        }
    }
}
