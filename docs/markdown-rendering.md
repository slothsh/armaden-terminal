# Chat Markdown and image settings

Chat rendering parses provider text for presentation only. Stored conversation text remains Markdown source. `md4c` builds the structured block/inline representation; fenced code is rendered as monospaced text with a language label, copy control, and wrapping/horizontal-scroll toggle. Syntax tokenization is not enabled. CTRE is accessed only through the project regex module.

Remote Markdown images are loaded asynchronously and lazily through the application image service. The service accepts HTTP and HTTPS, permits local/private/LAN hosts, validates downloaded JPEG/PNG/GIF data, and exposes only managed cache files to QML. It sends no application cookies, credentials, or referrer. Because private/LAN hosts are allowed, viewing a Markdown message can cause a GET request to the referenced local network address.

Settings live under `markdown.images` in the existing JSON application configuration:

```json
{
  "markdown": {
    "images": {
      "allowHttp": true,
      "allowHttps": true,
      "allowJpeg": true,
      "allowPng": true,
      "allowGif": true,
      "maxConcurrentRequests": 4,
      "maxQueuedRequests": 64,
      "timeoutMilliseconds": 15000,
      "maxRedirects": 5,
      "maxGifFrames": 300,
      "maxResponseBytes": 10485760,
      "maxDecodedPixels": 41943040,
      "maxCacheBytes": 134217728
    }
  }
}
```

The defaults bound network activity, response/decode memory and disk cache use. Numeric values are validated on load; out-of-range settings report a configuration error and fall back to safe defaults. Disable individual schemes or image formats by setting their `allow...` value to `false`.
