"""
Script de configuración post-instalación — FirestormIA
Se ejecuta automáticamente en el primer inicio.
Configura la API key de Groq para la traducción automática.
"""
import sys, os, re, configparser, webbrowser, time

INSTALL_DIR = os.path.dirname(os.path.abspath(__file__))
PROXY_PATH  = os.path.join(INSTALL_DIR, "groq-translate.py")
CONFIG_PATH = os.path.join(INSTALL_DIR, "groq_config.ini")

def titulo(texto):
    print()
    print("  " + "─" * 46)
    print(f"  {texto}")
    print("  " + "─" * 46)

def ok(texto):
    print(f"  ✓  {texto}")

def info(texto):
    print(f"  →  {texto}")

def main():
    os.system("cls")
    print()
    print("  ╔══════════════════════════════════════════════╗")
    print("  ║                                              ║")
    print("  ║   FirestormIA — Configuración de traducción  ║")
    print("  ║                                              ║")
    print("  ╚══════════════════════════════════════════════╝")

    titulo("¿QUÉ ES ESTO?")
    print()
    print("  FirestormIA traduce automáticamente los mensajes")
    print("  de Second Life usando inteligencia artificial.")
    print()
    print("  Para funcionar necesita conectarse a Groq,")
    print("  un servicio de IA GRATUITO (75.000 caracteres/día).")
    print()
    print("  Solo necesitás crear una cuenta gratis una vez.")

    titulo("PASO 1 — Crear cuenta en Groq (es gratis)")
    print()
    print("  Se va a abrir el navegador en console.groq.com")
    print()
    print("  Una vez ahí:")
    print("    1. Creá tu cuenta (podés usar Google o email)")
    print("    2. Andá a la sección  API Keys")
    print("    3. Hacé clic en  Create API Key")
    print("    4. Copiá la key que aparece (empieza con gsk_...)")
    print()
    input("  Presioná ENTER para abrir el navegador... ")

    webbrowser.open("https://console.groq.com/keys")
    time.sleep(2)

    titulo("PASO 2 — Ingresar tu API key")
    print()
    print("  Pegá la key que copiaste de Groq.")
    print("  (clic derecho → pegar, o Ctrl+V)")
    print()

    groq_key = ""
    while True:
        groq_key = input("  Tu API key: ").strip()
        if re.match(r'^gsk_[A-Za-z0-9]{30,}$', groq_key):
            break
        print()
        print("  ! La key no parece válida (debe empezar con gsk_)")
        print("    Asegurate de copiarla completa e intentá de nuevo.")
        print()

    info("Verificando la key con Groq...")
    try:
        import urllib.request
        req = urllib.request.Request("https://api.groq.com/openai/v1/models")
        req.add_header("Authorization", f"Bearer {groq_key}")
        urllib.request.urlopen(req, timeout=10)
        ok("Key válida.")
    except Exception as e:
        print()
        print(f"  ! No se pudo validar la key: {e}")
        print("    Revisá que la copiaste completa.")
        input("  Presioná ENTER para salir...")
        sys.exit(1)

    info("Guardando configuración...")
    cfg = configparser.ConfigParser()
    cfg["groq"] = {"api_key": groq_key}
    with open(CONFIG_PATH, "w", encoding="utf-8") as f:
        cfg.write(f)
    ok("Configuración guardada.")

    titulo("¡LISTO!")
    print()
    print("  FirestormIA está configurado y listo para usar.")
    print()
    print("  Firestorm se va a abrir ahora.")
    print()
    print("  La traducción se activa con el botón [IA]")
    print("  que aparece en cada ventana de chat.")
    print()
    input("  Presioná ENTER para continuar... ")

if __name__ == "__main__":
    main()
