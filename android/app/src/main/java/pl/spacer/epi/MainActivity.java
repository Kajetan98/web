package pl.spacer.epi;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Bundle;
import android.view.WindowManager;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

/**
 * Thin shell around the EPI web app. The whole app is bundled in assets, so the
 * build runs with no network at all: monitoring, stored events and contacts work
 * offline. Web Bluetooth is not part of the WebView, so an external BLE sensor
 * needs the browser build — the app falls back to its generator here.
 */
public class MainActivity extends Activity {

    private static final String APP_URL = "file:///android_asset/site/aplikacja/index.html";

    private WebView webView;

    @SuppressLint("SetJavaScriptEnabled")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // A seizure detector that lets the screen sleep stops detecting.
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        webView = new WebView(this);
        WebSettings settings = webView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);          // localStorage keeps events and contacts
        settings.setAllowFileAccess(true);
        settings.setMediaPlaybackRequiresUserGesture(false);
        settings.setSupportZoom(false);
        webView.setWebViewClient(new WebViewClient());
        webView.setBackgroundColor(0xFF171D2B);

        setContentView(webView);

        if (savedInstanceState == null) {
            webView.loadUrl(APP_URL + "?lang=" + language());
        } else {
            webView.restoreState(savedInstanceState);
        }
    }

    private String language() {
        String tag = getResources().getConfiguration().locale.getLanguage();
        return "pl".equals(tag) ? "pl" : "en";
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        webView.saveState(outState);
    }

    @Override
    public void onBackPressed() {
        if (webView.canGoBack()) {
            webView.goBack();
        } else {
            super.onBackPressed();
        }
    }

    @Override
    protected void onDestroy() {
        if (webView != null) {
            webView.destroy();
            webView = null;
        }
        super.onDestroy();
    }
}
