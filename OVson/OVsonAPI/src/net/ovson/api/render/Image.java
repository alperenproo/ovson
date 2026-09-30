package net.ovson.api.render;

public class Image {
    private final String url;
    private int textureId = -1;
    private boolean loaded = false;
    private boolean failed = false;

    public Image(String url) {
        this.url = url;
        loadAsync();
    }

    private void loadAsync() {
        new Thread(() -> {
            try {
                int id = net.ovson.api.render.gl.GL.loadTexture(url);
                if (id > 0) {
                    this.textureId = id;
                    this.loaded = true;
                } else {
                    this.failed = true;
                }
            } catch (Exception e) {
                this.failed = true;
            }
        }).start();
    }

    public void draw(float x, float y, float width, float height) {
        if (loaded && textureId != -1) {
            RenderAPI.drawImage(textureId, x, y, width, height);
        }
    }

    public boolean isLoaded() {
        return loaded;
    }

    public boolean hasFailed() {
        return failed;
    }
}
