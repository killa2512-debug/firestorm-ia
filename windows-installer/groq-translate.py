#!/usr/bin/env python3
"""
FirestormIA — Proxy de traducción automática
Usa Groq API (Llama 3.3) para traducir mensajes de Second Life en tiempo real.
"""

import sys
import os
import json
import threading
import configparser
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
import urllib.request
import urllib.error

CONFIG_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "groq_config.ini")
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 5000

def load_api_key():
    cfg = configparser.ConfigParser()
    cfg.read(CONFIG_FILE, encoding="utf-8")
    return cfg.get("groq", "api_key", fallback="")

def translate(text, target_lang="es", source_lang="auto"):
    api_key = load_api_key()
    if not api_key:
        return text

    if source_lang == "auto":
        prompt = f"Translate the following text to {target_lang}. Reply ONLY with the translation, nothing else:\n\n{text}"
    else:
        prompt = f"Translate the following {source_lang} text to {target_lang}. Reply ONLY with the translation, nothing else:\n\n{text}"

    payload = json.dumps({
        "model": "llama-3.3-70b-versatile",
        "messages": [{"role": "user", "content": prompt}],
        "temperature": 0.3,
        "max_tokens": 1024,
    }).encode("utf-8")

    req = urllib.request.Request(
        "https://api.groq.com/openai/v1/chat/completions",
        data=payload,
        headers={
            "Content-Type": "application/json",
            "Authorization": f"Bearer {api_key}",
        },
        method="POST"
    )

    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            return data["choices"][0]["message"]["content"].strip()
    except Exception as e:
        print(f"[groq-translate] Error: {e}", file=sys.stderr)
        return text


class TranslateHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass

    def do_GET(self):
        parsed = urlparse(self.path)
        params = parse_qs(parsed.query)

        if parsed.path == "/translate":
            text = params.get("q", [""])[0]
            target = params.get("target", ["es"])[0]
            source = params.get("source", ["auto"])[0]

            if not text:
                self.send_response(400)
                self.end_headers()
                return

            translated = translate(text, target, source)

            response = json.dumps({
                "translatedText": translated,
                "detectedSourceLanguage": source,
            }).encode("utf-8")

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(response)))
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(response)

        elif parsed.path == "/health":
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.end_headers()
            self.wfile.write(b"OK")

        else:
            self.send_response(404)
            self.end_headers()

    def do_POST(self):
        self.do_GET()


if __name__ == "__main__":
    api_key = load_api_key()
    if not api_key:
        print("[groq-translate] ADVERTENCIA: No hay API key configurada.")
        print("[groq-translate] Ejecuta setup.py para configurar la clave.")

    server = HTTPServer(("127.0.0.1", PORT), TranslateHandler)
    print(f"[groq-translate] Servidor de traducción iniciado en http://127.0.0.1:{PORT}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("[groq-translate] Detenido.")
